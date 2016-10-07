// UCLA CS 111 Lab 1 main program

#include <errno.h>
#include <error.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <ctype.h>

#include "command.h"

static char const *program_name;
static char const *script_name;

static void
usage (void)
{
  error (1, 0, 
	 "usage: %s [-p] [-t | [-N <MAX_NUMBER_OF_SUBPROCESSES>]] SCRIPT-FILE", 
	 program_name);
}

static int
get_next_byte (void *stream)
{
  return getc (stream);
}

int
get_max_processes (const char *arg)
{
  const char *p;
  int isdigit = 1;
  int max_processes = 0;

  for(p = arg; p && *p; p++)
    if(!isdigit(*p))
      isdigit = 0;
   
  if(arg && isdigit)
    {
      max_processes = strtol(optarg, NULL, 10);
      if(!(errno == ERANGE && 
	   (max_processes == INT_MIN || max_processes == INT_MAX)) &&
	 max_processes >= 2)
	return max_processes;
    }
   
   fprintf(stderr, "%s: error in MAX_NUMBER_OF_SUBPROCESSES\nAt least 2 processes required for time travel mode.", program_name);
   usage();

   return -1;
}

int
main (int argc, char **argv)
{
  int opt;
  int command_number = 1;
  int print_tree = 0;
  int time_travel = 0;
  int max_processes = 0;
  program_name = argv[0];

  for (;;)
    switch (getopt (argc, argv, "ptN:"))
      {
      case 'p': print_tree = 1; break;
      case 't': time_travel = 1; break;
      case 'N': max_processes = get_max_processes(optarg); break;
      default: usage (); break;
      case -1: goto options_exhausted;
      }
 options_exhausted:;

  // There must be exactly one file argument.
  if (optind != argc - 1)
    usage ();

  script_name = argv[optind];
  FILE *script_stream = fopen (script_name, "r");
  if (! script_stream)
    error (1, errno, "%s: cannot open", script_name);
  command_stream_t command_stream =
    make_command_stream (get_next_byte, script_stream);

  command_t last_command = NULL;
  command_t command;
  
  if(time_travel)
    execute_timetravel_command_stream(command_stream, 
				      max_processes > 1 ? max_processes : -1);
  else if(!time_travel && max_processes > 0)
      error (1, errno, "%s: MAX_NUMBER_OF_SUBPROCESSES only valid when run in time travel mode\n", program_name);

  while ((command = read_command_stream (command_stream)))
    {
      if (print_tree && !time_travel && max_processes <= 0)
	{
	  printf ("# %d\n", command_number++);
	  print_command (command);
	}
      else
	{
	  last_command = command;
	  execute_command (command, time_travel);
	}
    }

  return print_tree || !last_command ? 0 : command_status (last_command);
}
