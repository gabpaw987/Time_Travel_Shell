// UCLA CS 111 Lab 1 command reading

#include "alloc.h"
#include "command.h"
#include "command-internals.h"

#include <error.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

int is_good_char(char c)
{
  if ((isalnum(c) || c == '!' || c == '%' || c == '+' || c == ',' ||
      c == '-' || c == '.' || c == '/' || c == ':' || c == '@' ||
      c == '^' || c == '_') && c != '\n')
    return 1;
  return 0;
}

void extract_word(char *buf, int* i, char *dest, size_t word_size)
{
  size_t j;
  for(j=0 ; buf[*i]; (*i)++)
    {
      if (buf[*i] == ' ' || buf[*i] == '\t' || buf[*i] == '\0' || 
	  buf[*i] == '\n' || buf[*i] == '|' || buf[*i] == '&' ||
	  buf[*i] == '(' || buf[*i] == ')' || buf[*i] == ';' ||
	  buf[*i] == '<' || buf[*i] == '>' || buf[*i] == '\\')
	{
	  //(*i)--;
	  break;
	}
      else if(is_good_char(buf[*i]))
	{
	  if(j >= word_size)
	    {
	      word_size*=2;
	      checked_grow_alloc(dest, &word_size);
	    }
	  dest[j] = buf[*i];
	}
      else
	{
	  error(1, 0, "Syntax Error");//error
	}
      j++;
    }
}

void make_simple_command (command_t command, char *buf, int *i_ptr)
{
  if(!strlen(buf))
    {
       error(1, 0, "Syntax Error");
    }
  command->type = SIMPLE_COMMAND;
  command->u.word = (char**)checked_malloc(sizeof(char*)*16);
  size_t word_size = 16;
  size_t current_word = 0;
  int i = 0;

  if(i_ptr != NULL)
    i = *i_ptr;
  
  for (; buf[i]; i++)
    {
      if (buf[i] == '\n' || buf[i] == '|' || buf[i] == '&' ||
	  buf[i] == '(' || buf[i] == ')' || buf[i] == ';')
	{
	  //i--;
	  break;
	}
      else if (buf[i] == '<')
	{
	  if(!command->u.word[0] || command->input || command->output)
	    {
	      error(1, 0, "Syntax Error");
	    }
	  command->input = checked_malloc(sizeof(char*)*16);
	  i++;
	  if(buf[i] && buf[i] == ' ')
	    {
	      i++;
	    }
	  extract_word(buf, &i, command->input, 16);
	  if(!strlen(command->input))
	    error(1, 0, "Syntax Error");
	  i--;
	}
      else if (buf[i] == '>')
	{
	  if(!command->u.word[0] || command->output)
	    {
	      error(1, 0, "Syntax Error");
	    }
	  command->output = checked_malloc(sizeof(char*)*16);
	  i++;
	  if(buf[i] && buf[i] == ' ')
	    {
	      i++;
	    }
	  extract_word(buf, &i, command->output, 16);
	  if(!strlen(command->output))
	    error(1, 0, "Syntax Error");
	  i--;
	}
      else if (is_good_char(buf[i]) && buf[i] != ' ' && buf[i] != '\t')
	{
	  if(command->input || command->output)
	    {
	      error(1, 0, "Syntax Error");
	    }
	  command->u.word[current_word] = checked_malloc(sizeof(char*)*16);
	  extract_word(buf, &i, command->u.word[current_word++], word_size);
	  i--;
	}
      else if (buf[i] == '\\' && buf[i+1] && buf[i+1] == '\n')
	{
	  i++;
	}
      else if (buf[i] != ' ' && buf[i] != '\t')
	{
	  error(1, 0, "Syntax Error");
	}
    }

  if(i_ptr != NULL)
    {
      *i_ptr = i;
    }
}

int has_op_precedence(char new_op, char old_op)
{
  if((new_op == '|' && old_op != '|') ||
     ((new_op == '\233' || new_op == '\234') && old_op != '\233' && old_op != '\234' && old_op != '|') ||
     new_op == '(')
    return 1;
  return 0;
}

void combine_command (command_t command, command_t command_one, command_t command_two, char operator)
{ 
  if((operator == ')' || operator == '(') && command_two)
    {
      if(!command_two)
	error(1, 0, "Syntax Error");
      command->type = SUBSHELL_COMMAND;
      command->u.subshell_command = command_two;
      //kill command
    } 
  else 
    {
      if(!command_one || !command_two)
	error(1, 0, "Syntax Error");
      if (operator == '|' && 
	  (command_one->type == SIMPLE_COMMAND || 
	   command_one->type == PIPE_COMMAND || 
	   command_one->type == SUBSHELL_COMMAND) && 
	  (command_two->type == SIMPLE_COMMAND ||
	   command_two->type == PIPE_COMMAND|| 
	   command_two->type == SUBSHELL_COMMAND))
	{
	  command->type = PIPE_COMMAND;
	}
      else if (operator == '\233' && ((command_one->type == PIPE_COMMAND || 
				       command_one->type == SIMPLE_COMMAND ||
				       command_one->type == AND_COMMAND ||
				       command_one->type == OR_COMMAND ||
				       command_one->type == SUBSHELL_COMMAND) &&
				      (command_two->type == PIPE_COMMAND || 
				       command_two->type == SIMPLE_COMMAND ||
				       command_two->type == AND_COMMAND ||
				       command_two->type == OR_COMMAND ||
				       command_two->type == SUBSHELL_COMMAND)))
	{
	  command->type = AND_COMMAND;
	}
      else if (operator == '\234' && ((command_one->type == PIPE_COMMAND || 
				       command_one->type == SIMPLE_COMMAND ||
				       command_one->type == AND_COMMAND ||
				       command_one->type == OR_COMMAND ||
				       command_one->type == SUBSHELL_COMMAND) 
				      && (command_two->type == PIPE_COMMAND || 
				       command_two->type == SIMPLE_COMMAND ||
				       command_two->type == AND_COMMAND ||
				       command_two->type == OR_COMMAND ||
				       command_two->type == SUBSHELL_COMMAND)))
	{
	  command->type = OR_COMMAND;
	}
      else if ((operator == ';' || operator == '\n'))
	{
	  command->type = SEQUENCE_COMMAND;
	}
      else
	{
	  error(1, 0, "Syntax Error");
	}
	
      command->u.command[0] = command_one;
      command->u.command[1] = command_two;
      //kill commandss
    }
}

command_t* make_command_stack (char* buf, int* no_of_commands)
{
  command_t* command_stack = checked_malloc(sizeof(command_t)*(*no_of_commands));
  int current_command = 0;
  
  int no_of_operators = 64;
  char* operator_stack = checked_malloc(sizeof(char)*no_of_operators);
  int current_operator = 0;
  char just_read;
  int in_parenthesis = 0;

  int i;
  for (i = 0; buf[i]; i++)
    {
      if(current_operator == no_of_operators)
	{
	  no_of_operators *= 2;
	  operator_stack = checked_realloc(operator_stack, 
					   sizeof(char)*no_of_operators);
	}
      if(current_command == *no_of_commands)
	{
	  command_t* temp_command_stack = 
	    (command_t*) checked_malloc(sizeof(command_t)*(*no_of_commands)*2);
	  int j;
	  for(j = 0; j < *no_of_commands; j++)
	    {
	      temp_command_stack[j] = command_stack[j];
	    }
	  *no_of_commands *= 2;
	  //free(command_stack);
	  command_stack = temp_command_stack;
	}

      if(buf[i+1])
	{
	  if(buf[i] == '&' && buf[i+1] == '&')
	    {
	      i++;
	      buf[i] = '\233';
	    }
	  else if (buf[i] == '|' && buf[i+1] == '|')
	    {
	      i++;
	      buf[i] = '\234';
	    }
	}
	
      if(is_good_char(buf[i]))
	{
	  if(buf[i] == '\\' && buf[i+1] && buf[i+1] == '\n')
	    i+=2;
	  if(!is_good_char(buf[i]))
	    {
	      i--;
	      continue;
	    }
	  
	  command_t new_command = checked_malloc(sizeof(struct command));
	  //new_command->input = NULL;
	  //new_command->output = NULL;
	  make_simple_command(new_command, buf, &i);
	  command_stack[current_command] = new_command;
	  i--;
	  current_command++;
	  just_read = 'w';
	}
      else if ((buf[i] == ';' || buf[i] == '|' || 
		buf[i] == '\233' || buf[i] == '\234' ||
		buf[i] == '(' || buf[i] == ')' ||
		buf[i] == '<' || buf[i] == '>' ||
		buf[i] == '\n') && 
	       (just_read != '\n' || buf[i] == '\n'|| 
		buf[i] == '(' || buf[i] == ')'))
	{
	  int did_break = 0;
	  int tempI = 0;

	  if (buf[i] == '(')
	    in_parenthesis++;
	  else if (just_read == '\n' && buf[i] != '\n' && buf[i] != ')')
	    error(1, 0, "Syntax Error");

	  if((current_operator == 0) ||
	     has_op_precedence(buf[i], operator_stack[current_operator-1]))
	      {
		if(!(just_read == '\n' && buf[i] == '\n'))
		  {
		    operator_stack[current_operator] = buf[i];
		    current_operator++;
		    just_read = buf[i];
		  }
	      }
	  else		  
	      {
		int realI = i;
		tempI = 0;
		if(buf[i] == ';' && just_read == '(')
		  {
		    error(1, 0, "Syntax Error");
		  }
		while (current_operator > 0 &&
		       (operator_stack[current_operator-1] != '(' || 
			buf[i] == ')') &&
		       !has_op_precedence(buf[i], 
					  operator_stack[current_operator-1]))
		  {
		    //realloc command array
		    char operator = operator_stack[--current_operator];
		    current_command--;
		    command_t command_two = command_stack[current_command];
		    command_t command_one;
		    
		    if (buf[i] == ')' && 
			(just_read == ';' || just_read == '\n'))
		      {
			current_command++;
			just_read = 'w';
			continue;
		      }
		    if ((just_read == '\n' || just_read == '\233' || 
			      just_read == '\234' || just_read == ';' || 
			      just_read == '(' || just_read == '|') &&
			     current_command >= 0 && 
			     buf[i] == '\n' && 
			     (!in_parenthesis || operator == ';' || operator == '\n'))
		      {
			if((operator != '\n' && 
			    operator != ';') || in_parenthesis)
			  current_operator++;
			current_command++;
			did_break = 1;
			break;
		      }
		    /*else if(current_command > 0 && (buf[i] == ';' || 
						      buf[i] == '\n') &&
		      (command_stack[current_command-1]->type == 
		       SEQUENCE_COMMAND ||
		       command_stack[current_command-1]->type == OR_COMMAND ||
		       command_stack[current_command-1]->type == AND_COMMAND))
		      {
			
		      }*/
		    /*else if (just_read != 'w' && just_read != ';' && 
			       just_read != ')' && !in_parenthesis && 
			       buf[i] == '\n')
		      {
			current_command++;
			current_operator++;
			did_break = 1;
			break;
		      }*/
		    /*else if (operator == '\n' && 
			     in_parenthesis)
		      {
			//current_operator++;
			current_command++;
			did_break = 1;
			continue;
			}*/
		    else if ((buf[i] != ')' && current_command <= 0) || 
			     current_command < 0 || 
			     (just_read != 'w' && operator != '\n'))
		      {
			command_one=NULL;
			error(1, 0, "Syntax Error");
		      }
		    else if (operator != '(')
		      {
			command_one = command_stack[--current_command];
		      }

		    command_t new_command = checked_malloc(sizeof(struct command));
		    combine_command(new_command, command_one, command_two, operator); 
		    command_stack[current_command] = new_command;
		    current_command++;

		    if(operator == '(')
		      {
			tempI = i;
			i++;
			for (; buf[i]; i++)
			  {
			    if (buf[i] == '\n' || buf[i] == '|' || 
				buf[i] == '&' || buf[i] == '(' || 
				buf[i] == ')' || buf[i] == ';')
			      {
				realI = i;
				i=tempI;
				break;
			      }
			    else if (buf[i] == '<')
			      {
				if(!new_command->u.subshell_command || 
				   new_command->input || new_command->output)
				  {
				    error(1, 0, "Syntax Error");
				  }
				new_command->input = 
				  checked_malloc(sizeof(char*)*16);
				i++;
				if(buf[i] && buf[i] == ' ')
				  {
				    i++;
				  }
				extract_word(buf, &i, new_command->input, 16);
				i--;
			      }
			    else if (buf[i] == '>')
			      {
				if(!new_command->u.subshell_command || 
				   new_command->output)
				  {
				    error(1, 0, "Syntax Error");
				  }
				new_command->output = 
				  checked_malloc(sizeof(char*)*16);
				i++;
				if(buf[i] && buf[i] == ' ')
				  {
				    i++;
				  }
				extract_word(buf, &i, new_command->output, 16);
				i--;
			      }
			    else if (buf[i] == '\\' && buf[i+1] && 
				     buf[i+1] == '\n')
			      {
				i++;
			      }
			    else if (buf[i] != ' ' && buf[i] != '\t')
			      {
				error(1, 0, "Syntax Error");
			      }
			  }
			in_parenthesis--;
			break;
		      }
		    //kill command
		  }
		
		if(((!did_break && buf[i] != ')') || 
		    buf[i] == ';') && 
		   !((just_read == '(' || just_read == ';') &&
		      buf[i] == '\n'))
		  operator_stack[current_operator++] = buf[i];

		if(buf[i] == ')')
		  {
		    realI--;
		    if(buf[realI] == ' ')
		      realI--;
		  }
		i = realI;
	      }
	  if(!tempI || buf[tempI] != ')')
	    just_read = buf[i];
	  else
	    just_read = 'w';
	}
      else if (buf[i] != ' ' && buf[i] != '\t')
	{
	  error(1, 0, "Syntax Error");
	}
    }
  if(current_operator > 1 || (current_operator == 1 && operator_stack[current_operator-1] != '\n') || in_parenthesis != 0)
    {
      error(1, 0, "Syntax Error");
    }
  *no_of_commands = current_command;
  free(operator_stack);
  return command_stack;
}

command_stream_t
make_command_stream (int (*get_next_byte) (void *),
		     void *get_next_byte_argument)
{
  size_t no_of_chars = 2048;
  char* buf = checked_malloc(sizeof(char)*no_of_chars);
  size_t current_char = 0;

  char new_char;
  while (new_char > -1)
    {
      char old_char = new_char;
      new_char = get_next_byte(get_next_byte_argument);

      if(new_char == '#' && !(old_char && (old_char == ';' || old_char == '|' ||
	       old_char == '\233' || old_char == '\234' ||
	       old_char == '(' || old_char == ')' ||
	       old_char == '<' || old_char == '>')))
	{
	  while (new_char > -1 && new_char != '\n')
	    {
	      new_char = get_next_byte(get_next_byte_argument);
	    }
	}

      if(new_char == '\n' && current_char == 0)
	continue;

      if (new_char > -1)
	{
	  if (current_char+1 >= no_of_chars)
	    {
	      no_of_chars*=2;
	      buf = checked_grow_alloc(buf, &no_of_chars);
	    }
	  buf[current_char++] = new_char;
	}
    }
  buf[current_char++] = '\n';

  command_stream_t my_command_stream = checked_malloc(sizeof(struct command_stream));

  int no_of_commands = 16;
  command_t* command_stack = make_command_stack(buf, &no_of_commands);
  
  int i;
  command_stream_node_t old_node = NULL;
  for(i = 0; i < no_of_commands; i++)
    {
      command_stream_node_t new_node = checked_malloc(sizeof(struct command_stream_node));
      new_node->prev_command = old_node;
      
      new_node->command = command_stack[i];
      
      if(i == 0)
	my_command_stream->root = new_node;
      else
	old_node->next_command = new_node;
      
      old_node = new_node;
    }

  free(command_stack);

  return my_command_stream;
}

void 
free_command(command_t command)
{
  if(command->input)
    free(command->input);
  if(command->output)
    free(command->output);
  
  if(command->type == SIMPLE_COMMAND)
    free(command->u.word);
  else if (command->type == SUBSHELL_COMMAND)
    free_command(command->u.subshell_command);
  else
    {
      free_command(command->u.command[0]);
      free_command(command->u.command[1]);
    }
  
  free(command);
}

command_t
read_command_stream (command_stream_t s)
{
  if (s != NULL && s->root != NULL)
    {
      command_stream_node_t node = s->root;
      s->root = node->next_command;
      if(node->prev_command != NULL)
	{
	  free_command(node->prev_command->command);
	  free(node->prev_command);
	}
      return node->command;
    }
  return NULL;
}
