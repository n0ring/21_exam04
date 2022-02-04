#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define STD_OUT 1
#define STD_IN	0
#define STD_ERR 2

#define SIDE_OUT 0
#define SIDE_IN 1

#define TYPE_END 0
#define TYPE_PIPE 1
#define TYPE_BREAK 2

typedef struct s_list
{
	char			**args;
	int				length;
	int				type;
	int				pipes[2];
	struct s_list	*next;
}					t_list;

int	ft_strlen(char *str)
{
	int i;
	
	i = 0;
	while (str[i])
		i++;
	return (i);
}

void clear_list(t_list *list)
{
	t_list *tmp;
	char	**str_p;
	
	// printf("CLEAR\n");
	while (list)
	{
		tmp = list;
		str_p = list->args;
		while (str_p && *str_p)
		{
			// printf("arg: %s\n", *str_p);
			str_p++;
		}
		if (list->args)
			free(list->args);
		// printf("block type %s %d\n", list->type == TYPE_BREAK ? "TYPE_BREAK" : "TYPE_PIPE", list->type);
		list = tmp->next;
		free(tmp);
	}
}

void exit_fatal(char *msg)
{
	write(STD_ERR, msg, ft_strlen(msg));
	exit (EXIT_FAILURE);
}


t_list *to_last_el(t_list *list)
{
	t_list *tmp;

	tmp = list;
	while (tmp->next)
		tmp = tmp->next;
	return (tmp);
}

int get_cmds_in_block(char **arg, int i)
{
	int res;

	res = 0;
	while (arg[i] && strcmp(arg[i], ";") && strcmp(arg[i], "|"))
	{
		i++;
		res++;
	}
	return (res);
}

void	parse_args(t_list **list,  char **arg, int *i)
{
	int			inner_i;
	int			k;
	t_list		*current_el;
	int			cmds_in_block;

	inner_i = *i;

	if (!strcmp(arg[inner_i], ";") || !strcmp(arg[inner_i], "|"))
	{
		// printf("IN ;| branch\n");
		if (*list)
		{
			current_el = to_last_el(*list);
			current_el->type = (!strcmp(arg[inner_i], ";")) ? TYPE_BREAK : TYPE_PIPE;
		}
		(*i)++;
		return ;
	}
	cmds_in_block = get_cmds_in_block(arg, inner_i);
	if (cmds_in_block == 0)
	{
		(*i)++;
		return ;
	}
	current_el = malloc(sizeof(t_list));
	if (!current_el)
		exit_fatal("error: fatal\n");
	current_el->args = malloc((cmds_in_block + 1) * sizeof(char *));
	if (!current_el->args)
		exit_fatal("error: fatal\n");
	(current_el->args)[cmds_in_block] = NULL;
	// printf("cmd in block %d\n", cmds_in_block);
	*i = *i + cmds_in_block;
	k = 0;
	while (k < cmds_in_block)
	{
		(current_el->args)[k] = arg[inner_i];
		k++;
		inner_i++;
	}
	if (*list)
		to_last_el(*list)->next = current_el;
	else
		*list = current_el;
	current_el->type = TYPE_END;
	current_el->pipes[0] = -2;
	current_el->pipes[1] = -2;
}

int cd_builtin(t_list *el)
{
	int	args_count;

	args_count = 0;
	while (el->args[args_count])
		args_count++;
	if (args_count != 2)
	{
		write(STD_ERR, "error: cd: bad arguments\n", ft_strlen("error: cd: bad arguments\n"));
		return (EXIT_FAILURE);
	}
	if (chdir(el->args[1]))
	{
		write(STD_ERR, "error: cd: cannot change directory to ", ft_strlen("error: cd: cannot change directory to "));
		write(STD_ERR, el->args[1], ft_strlen(el->args[1]));
		write(STD_ERR, "\n", 1);
		return (EXIT_FAILURE);
	}	
	return (EXIT_SUCCESS);
}

int exec(t_list *el, int input_fd, char **env)
{
	int pid;
	int exit_status;
	int last_exit_code;

	last_exit_code = 0;
	if (el->type == TYPE_PIPE)
		pipe(el->pipes);
	pid = fork();
	if (pid == -1)
		exit_fatal("error: fatal\n");
	if (pid == 0)
	{
		if (el->type == TYPE_PIPE)
			dup2(el->pipes[1], STD_OUT);
		if (input_fd != -1)
			dup2(input_fd, STD_IN);
		execve(el->args[0], el->args, env);
		write(2, "error: cannot execute ", ft_strlen("error: cannot execute "));
		write(2, el->args[0], ft_strlen(el->args[0]));
		write(2, "\n", 1);
		exit(1);
	}
	else
	{
		if (el->type == TYPE_PIPE)
			close(el->pipes[1]);
		waitpid(pid, &exit_status, 0);
		if (WIFEXITED(exit_status))
			last_exit_code = WEXITSTATUS(exit_status);
		printf("exit code %d\n", last_exit_code);
		if (input_fd != -1)
			close(input_fd);
	}
	if (el->type == TYPE_PIPE)
		return (el->pipes[0]);
	if (!el->next)
		return (last_exit_code);
	return (-1);
}

int exec_part(t_list *list, char **env)
{
	int input_fd;

	input_fd = -1;

	if (!list)
		return (1);
	while (list)
	{
		if (!strcmp(list->args[0], "cd"))
		{
			cd_builtin(list);
			input_fd = -1;
			list = list->next;
			continue ;
		}
		input_fd = exec(list, input_fd, env);
		list = list->next;
	}
	return (input_fd);
}



int main(int argc, char **argv, char **env)
{
	int		last_exit_code;
	t_list	*cmds;
	int		i;

	last_exit_code = EXIT_SUCCESS;
	cmds = NULL;
	i = 1;
	while (i < argc)
		parse_args(&cmds, argv, &i);
	last_exit_code = exec_part(cmds, env);
	clear_list(cmds);
	return (last_exit_code);
}