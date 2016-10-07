// UCLA CS 111 Lab 1 command execution

#include "command.h"
#include "command-internals.h"
#include "alloc.h"

#include <error.h>
#include <sys/types.h>
#include <sys/wait.h> 
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>  
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <limits.h>
#include <sys/mman.h>

enum dependency_type
  {
    INPUT,
    OUTPUT,
    UNDEF,
  };

typedef struct dependency_list_node *dependency_list_node_t;
struct dependency_list_node {
  char *file_name;
  enum dependency_type type;
  int no_of_pids;
  int max_pids;
  int* pids;
  dependency_list_node_t next_node;
};

pthread_mutex_t max_processes_mutex;

void
execute_general_command(command_t command, int* num_processes_running, 
			int max_processes);

int
command_status (command_t c)
{
  return c->status;
}

dependency_list_node_t
find_dependency_node (char* dep_name, dependency_list_node_t dep_root, 
		      enum dependency_type dep_type)
{
  dependency_list_node_t cur_dep = dep_root;
  while(cur_dep)
    {
      if(strcmp(dep_name, cur_dep->file_name) == 0 && cur_dep->type == dep_type)
	return cur_dep;
      if(cur_dep->next_node)
	cur_dep = cur_dep->next_node;
      else
	break;
    }
  return cur_dep;
}

dependency_list_node_t
add_nonexistant_dep(char* dep_name, dependency_list_node_t dep_root, 
		    enum dependency_type dep_type)
{
  dependency_list_node_t cur_dep = find_dependency_node(dep_name, 
							 dep_root, 
							 dep_type);
  if(cur_dep && (strcmp(dep_name, cur_dep->file_name) != 0 || 
		 cur_dep->type != dep_type))
    {
      cur_dep->next_node = checked_malloc(sizeof(struct dependency_list_node));
      cur_dep = cur_dep->next_node;
    }
  else if(!cur_dep)
    {
      cur_dep = checked_malloc(sizeof(struct dependency_list_node));
    }
  else
    {
      return dep_root;
    }

  cur_dep->file_name = dep_name;
  cur_dep->type = dep_type;
  cur_dep->no_of_pids = 0;
  cur_dep->max_pids = 16;
  cur_dep->pids = checked_malloc(cur_dep->max_pids*sizeof(int));
  int i;
  for(i = 0; i < cur_dep->max_pids; i++)
    cur_dep->pids[i] = -1;
  cur_dep->next_node = NULL;
  
  if(!dep_root)
    return cur_dep;
  
  return dep_root;
}

dependency_list_node_t
find_command_dependencies (dependency_list_node_t dep_root, 
			   command_t cur_command)
{
  if(cur_command)
    {
      if(cur_command->input)
	dep_root = add_nonexistant_dep(cur_command->input, dep_root, INPUT);
      if(cur_command->output)
	dep_root = add_nonexistant_dep(cur_command->output, dep_root, OUTPUT);
      int i;
      switch(cur_command->type)
	{
	case SIMPLE_COMMAND:
	  for(i = 0; cur_command->u.word[i]; i++)
	      dep_root = add_nonexistant_dep(cur_command->u.word[i], 
					     dep_root, 
					     UNDEF);
	  break;
	case AND_COMMAND:
	case OR_COMMAND:
	case SEQUENCE_COMMAND:
	case PIPE_COMMAND:
	  dep_root = find_command_dependencies(dep_root, 
					       cur_command->u.command[0]);
	  dep_root = find_command_dependencies(dep_root, 
					       cur_command->u.command[1]);
	  break;
	case SUBSHELL_COMMAND:
	  dep_root = find_command_dependencies(dep_root, 
					       cur_command->u.subshell_command);
	  break;
	}
    }
  return dep_root;
}

dependency_list_node_t
find_all_dependencies (command_stream_t command_stream)
{
  dependency_list_node_t dep_root = NULL;
  command_stream_node_t cur_command_node = command_stream->root;
  
  while(cur_command_node)
    { 
      dep_root = find_command_dependencies(dep_root, cur_command_node->command);
      cur_command_node = cur_command_node->next_command;
    }
  
  return dep_root;
}

void
free_dep_list(dependency_list_node_t dep_root)
{
  dependency_list_node_t old_node = NULL;
  do
    {
      if(old_node)
	{
	  if(old_node->pids)
	    free(old_node->pids);
	  free(old_node);
	}
      old_node = dep_root;
      if(dep_root)
	dep_root = dep_root->next_node;
    } while (dep_root);
}

int*
check_command_dependencies(command_t command, dependency_list_node_t dep_root,
			   int caller_pid)
{
  dependency_list_node_t specific_command_dep_root = NULL;
  specific_command_dep_root = 
    find_command_dependencies(specific_command_dep_root, command);
  dependency_list_node_t cur_spec_node = specific_command_dep_root;
  dependency_list_node_t tmp_node = cur_spec_node;
  int waiting_for_size = 16;
  int* waiting_for = checked_malloc(waiting_for_size * sizeof(int));
  int i;
  for (i = 0; i < waiting_for_size; i++)
    waiting_for[i] = -1;
  i = 0;
  while (cur_spec_node)
    {
      int y;
      for (y = 0; y < 3; y++)
	{
	  enum dependency_type dep_type;
	  switch (y)
	    {
	    case 0:
	      dep_type = INPUT;
	      break;
	    case 1:
	      dep_type = OUTPUT;
	      break;
	    case 2:
	      dep_type = UNDEF;
	      break;
	    }
	  dependency_list_node_t cur_dep_node = 
	    find_dependency_node(cur_spec_node->file_name, 
				 dep_root, dep_type);
	  
	  if(strcmp(cur_dep_node->file_name, cur_spec_node->file_name) == 0)
	    {
	      if(cur_dep_node->type == cur_spec_node->type)
		{
		  if(cur_dep_node->max_pids <= cur_dep_node->no_of_pids)
		    {
		      cur_dep_node->pids = 
			checked_realloc(cur_dep_node->pids, 
					cur_dep_node->max_pids*2*sizeof(int));
		      int j;
		      for(j = cur_dep_node->max_pids; 
			  j < cur_dep_node->max_pids*2; j++)
			cur_dep_node->pids[j] = -1;
		      cur_dep_node->max_pids *= 2;
		    }
		  cur_dep_node->pids[cur_dep_node->no_of_pids++] = caller_pid;
		}
	      if(!((cur_dep_node->type == INPUT ||
		    cur_dep_node->type == UNDEF) &&
		   (cur_spec_node->type == INPUT || 
		    cur_spec_node->type == UNDEF)))
		{
		  //fprintf(stderr, "waiting for: %s\n", 
		  //	  cur_dep_node->file_name);
		  int j;
		  for(j = 0; j < cur_dep_node->no_of_pids && 
			cur_dep_node->pids[j] != caller_pid; j++)
		    {
		      int duplicate = 0;
		      int m;
		      for(m = 0; m <= i; m++)
			if(waiting_for[m] == cur_dep_node->pids[j])
			  duplicate = 1;
		      if(!duplicate)
			{
			  if(waiting_for_size <= i)
			    {
			      waiting_for = 
				checked_realloc(waiting_for, 
						waiting_for_size * sizeof(int));
			      int k;
			      for(k = waiting_for_size; k < 2*waiting_for_size; 
				  k++)
				waiting_for[k] = -1;
			      waiting_for_size *= 2;
			    }
			  
			  waiting_for[i++] = cur_dep_node->pids[j];
			  
			  if(cur_dep_node->type == cur_spec_node->type)
			    {
			      int k;
			      cur_dep_node->no_of_pids--;
			      for(k = j; k < cur_dep_node->no_of_pids; k++)
				cur_dep_node->pids[k] = cur_dep_node->pids[k+1];
			      cur_dep_node->pids[cur_dep_node->no_of_pids] = -1;
			    }
			}
		    }     
		}
	    }
	}
      cur_spec_node = cur_spec_node->next_node;
    }
  free_dep_list(specific_command_dep_root);
  /*int x;
  for(x = 0; x < 3; x++)
  fprintf(stderr, "waiting_list: %d\n", waiting_for[x]);*/
  return waiting_for;
}

void
remove_pid_from_remaining_dependencies(dependency_list_node_t dep_root,
					command_t command,
					int caller_pid)
{
  int i, j;
  while(dep_root)
    {
      for(j = 0; j < dep_root->no_of_pids; j++)
	{
	  if(dep_root->pids[j] == caller_pid)
	    {
	      dep_root->no_of_pids--;
	      for(i = j; i < dep_root->no_of_pids; i++)
		dep_root->pids[i] = dep_root->pids[i+1];
	      dep_root->pids[i] = -1;
	    }
	}
      dep_root = dep_root->next_node;
    }
}

void
execute_and_command(command_t command, int* num_processes_running, 
		    int max_processes)
{
  execute_general_command(command->u.command[0],
			  num_processes_running, max_processes);
  if(!command->u.command[0]->status)
    {
      execute_general_command(command->u.command[1],
			      num_processes_running, max_processes);
      command->status = command->u.command[1]->status;
    }
  else
    command->status = command->u.command[0]->status;
}

void 
execute_or_command(command_t command, int* num_processes_running, 
		   int max_processes)
{
  execute_general_command(command->u.command[0],
			  num_processes_running, max_processes);
  if(command->u.command[0]->status)
    {
      execute_general_command(command->u.command[1],
			      num_processes_running, max_processes);
      command->status = command->u.command[1]->status;
    }
  else
    command->status = command->u.command[0]->status;
}

void
execute_simple_or_subshell_command(command_t command,int* num_processes_running,
			int max_processes)
{
  if(command->type == SUBSHELL_COMMAND)
    {
      if(command->input)
	freopen(command->input, "r", stdin);
      if(command->output)
	freopen(command->output, "w", stdout);
      execute_general_command(command->u.subshell_command,
			      num_processes_running, max_processes);
      //fclose(fp1);
      //fclose(fp2);
    }
  else
    {
      //fprintf(stderr, "process opened: %d\n", *num_processes_running);
      pid_t pid = fork();
      if(pid > 0)
	{
	  int status;
	  if(waitpid(pid, &status, 0) == -1)
	    error(1, errno, "Error while exiting child process!");
	  command->status = WEXITSTATUS(status);
	  /*if(num_processes_running)
	    {
	      pthread_mutex_lock(&max_processes_mutex);
	      (*num_processes_running)--;
	      pthread_mutex_unlock(&max_processes_mutex);
	      //fprintf(stderr, "process closed: %d\n", *num_processes_running);
	    }*/
	  //
	  
	  /*if(!command->input)
	    fclose(stdin);*/
	}
      else if(!pid)
	{
	  if(command->input)
	    freopen(command->input, "r", stdin);
	  if(command->output)
	    freopen(command->output, "w", stdout);
	  //fprintf(stderr, "ex: %s\n", command->u.word[0]);
	  execvp(command->u.word[0], command->u.word);
	  error(1, 0, "Error while executing simple or subshell command!");
	}
      else
	{
	  error(1, 0, "Error while forking!");
	}
    }
}

void
execute_sequence_command(command_t command, int* num_processes_running, 
			int max_processes)
{
  execute_general_command(command->u.command[0],
			  num_processes_running, max_processes);
  execute_general_command(command->u.command[1],
			  num_processes_running, max_processes);
  command->status = WEXITSTATUS(command->u.command[1]);
}

void
execute_pipe_command(command_t command, int* num_processes_running, 
			int max_processes)
{
  int status, num_running;
  int pipe_fd[2];

  if(num_processes_running && *num_processes_running > max_processes)
    {
      int fd;
      pthread_mutex_lock(&max_processes_mutex);
      (*num_processes_running) -= 2;
      
      /*fprintf(stderr, "first %s\n", command->u.command[0]->u.command[0]->u.word[0]);
      //if(dup2(fd, 1) == -1)
      //error (1, errno,  "Error while using stream!");
      freopen("tmp", "w", stdout);
      execute_general_command(command->u.command[0],
			  num_processes_running, max_processes);
      fclose(stdout);
      //freopen("tmp", "r", stdin);
      fprintf(stderr, "second\n");
      //close(fd);
      //if(dup2(1, 1) == -1)
      //error (1, errno,  "Error while using stream!");
      execute_general_command(command->u.command[0],
			  num_processes_running, max_processes);
			  command->status = WEXITSTATUS(command->u.command[1]);*/
      execute_sequence_command(command, num_processes_running, max_processes);
      pthread_mutex_unlock(&max_processes_mutex);

      return;
    }

  if (pipe(pipe_fd) == -1)
    error(1, errno, "Error while creating a pipe!");

  if(num_processes_running)
    num_running = *num_processes_running;
  pid_t pid = fork();
  //fprintf(stderr, "process opened: %d\n", *num_processes_running);
 
  if(!pid)
    {
      close(pipe_fd[0]);
      if(dup2(pipe_fd[1], 1) == -1)
	error (1, errno, "Error while using stream!");
      execute_general_command(command->u.command[0],
			      num_processes_running, max_processes);
      _exit(command->u.command[0]->status);
    }
  else if(pid > 0)
    {
      close(pipe_fd[1]);
      if(dup2(pipe_fd[0], 0) == -1)
	error (1, errno,  "Error while using stream!");
      execute_general_command(command->u.command[1],
			      num_processes_running, max_processes);
    }
  else if(pid < 0)
    {
      error(1, 0, "Error while forking!");
    }

  if(waitpid(pid, &status, 0) == -1)
    error(1, errno, "Error while exiting child process!");
  command->status = WEXITSTATUS(command->u.command[1]->status);

  if(num_processes_running)
    {
      pthread_mutex_lock(&max_processes_mutex);
      (*num_processes_running) -= 2;
      pthread_mutex_unlock(&max_processes_mutex);
    }
}

void
execute_general_command(command_t command, int* num_processes_running, 
			int max_processes)
{
  switch(command->type)
  {
    case AND_COMMAND:
      execute_and_command(command, num_processes_running, max_processes);
      break;
    case OR_COMMAND:
      execute_or_command(command , num_processes_running, max_processes);
      break;
    case PIPE_COMMAND:
      execute_pipe_command(command, num_processes_running, max_processes);
      break;
    case SEQUENCE_COMMAND:
      execute_sequence_command(command, num_processes_running, max_processes);
      break;  
    case SIMPLE_COMMAND:
    case SUBSHELL_COMMAND:
      execute_simple_or_subshell_command(command, num_processes_running, 
					 max_processes);
      break;
    default:
      error(1, 0, "Unknown command type!");
  }
}

int
get_number_of_processes_required(command_t command)
{
  int number_of_processes = 0;
  int no_process_one = 0;
  int no_process_two = 0;
  
  switch(command->type)
  {
  case AND_COMMAND:
  case OR_COMMAND:
  case SEQUENCE_COMMAND:
    no_process_one = 
      get_number_of_processes_required(command->u.command[0]);
    no_process_two = 
      get_number_of_processes_required(command->u.command[1]);
    number_of_processes += (no_process_one > no_process_two) ? no_process_one :
      no_process_two;
    break;
  case SUBSHELL_COMMAND:
    number_of_processes += 
      get_number_of_processes_required(command->u.subshell_command);
    break;
  case PIPE_COMMAND:
    number_of_processes += 
      get_number_of_processes_required(command->u.command[0]) + 
      get_number_of_processes_required(command->u.command[1]) + 1;
      break;
  case SIMPLE_COMMAND:
    number_of_processes++;
    break;
  default:
      error(1, 0, "Unknown command type!");
  }
  return number_of_processes;
}

void
execute_command (command_t c, int time_travel)
{
  if (!time_travel)
    execute_general_command(c, NULL, INT_MAX);
}

void
execute_timetravel_command_stream (command_stream_t command_stream,
				   int max_processes)
{
  dependency_list_node_t dep_root = find_all_dependencies(command_stream);
  command_stream_node_t cur_cs_node = command_stream->root;
  int status = 0;
  pthread_mutex_t pid_mutex;
  command_stream_node_t tmp = command_stream->root;
  int cs_length;
  int running_processes = 0;
  for(cs_length = 0; tmp; cs_length++)
    tmp = tmp->next_command;
 
  int* pids = mmap(0, cs_length*sizeof(int), PROT_READ|PROT_WRITE, 
		   MAP_SHARED|MAP_ANONYMOUS, -1, 0);
  int* num_processes_running = mmap(0, sizeof(int), PROT_READ|PROT_WRITE, 
				    MAP_SHARED|MAP_ANONYMOUS, -1, 0);
  *num_processes_running = 0;
  //int* pids = checked_malloc(cs_length*sizeof(int));
  int id = -1;

  if(max_processes < 2)
    max_processes = INT_MAX;
  while(cur_cs_node)
    {
      id++;
      int processes_required = 
	get_number_of_processes_required(cur_cs_node->command) + 1;
      if(processes_required > max_processes)
	fprintf(stdout, "Insufficient max processes to run command %d in parallel. The last %d piped command(s) will be executed in sequence!\n", id, (processes_required - max_processes) / 2);
      else
	while(*num_processes_running + processes_required > max_processes)
	  sched_yield();
      pthread_mutex_lock(&max_processes_mutex);
      *num_processes_running += processes_required;
      pthread_mutex_unlock(&max_processes_mutex);
      //fprintf(stderr, "%d; %d; %d\n",*num_processes_running, 
      //processes_required, max_processes);
      pthread_mutex_lock(&pid_mutex);
      int* waiting_list = 
	check_command_dependencies(cur_cs_node->command, dep_root, id);
      
      int* waiting_list_ptr = waiting_list;

      //fprintf(stderr, "process opened: %d\n", *num_processes_running);
      int pid = fork();
      if(!pid) //child process
	{
	  //fprintf(stderr, "start: %d, %d\n", id, 
	  //	  cur_cs_node->command->type);

	  int i;
	  while(waiting_list_ptr && *waiting_list_ptr != -1)
	    {
	      //fprintf(stderr, "waiting for ex: %d\n", *waiting_list_ptr);
	      
	      while(pids[*waiting_list_ptr] > 0)
		{
		  sched_yield();
		  //waitpid(pids[*waiting_list_ptr], &status, WNOHANG);
		 }
	      waiting_list_ptr++;
	    }
       
	  execute_general_command(cur_cs_node->command, num_processes_running,
				  max_processes);
	  //fprintf(stderr, "finished command %d\n", id);
	  pthread_mutex_lock(&pid_mutex);
	  remove_pid_from_remaining_dependencies(dep_root,
						 cur_cs_node->command, 
						 id);
	  pids[id] = -1;
	  pthread_mutex_unlock(&pid_mutex);
	 
	  //fprintf(stderr, "end: %d, changed: %d\n", id, pids[id]);
	  free(waiting_list);
	  
	  pthread_mutex_lock(&max_processes_mutex);
	  *num_processes_running -= 2;
	  pthread_mutex_unlock(&max_processes_mutex);
	  //fprintf(stderr, "process closed: %d\n", *num_processes_running);

	  _exit(cur_cs_node->command->status);
	}

      //parent process      
      pids[id] = pid;
      pthread_mutex_unlock(&pid_mutex);

      cur_cs_node = cur_cs_node->next_command;
    }
  
  int l;
  for(l = 0; l < cs_length; l++)
    if(pids[l] != -1)
      if(waitpid(pids[l], &status, 0) > 0)
	pids[l] = -1;
  free_dep_list(dep_root);
  munmap(pids, sizeof(int)*cs_length);
  munmap(num_processes_running, sizeof(int));
}
