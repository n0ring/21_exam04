#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#define STD_OUT 1
#define STD_IN	0
#define STD_ERR 2

#define TYPE_END 0
#define TYPE_PIPE 1
#define TYPE_BREAK 2

#define ERR_FATAL "error: fatal"
#define CD_ERR_ARG "error: cd: bad arguments"
#define CD_ERR_CHDIR "error: cd: cannot change directory to"
#define EXEC_ERR "error: cannot execute"

typedef struct s_list
{
	char			**args;
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

void	show_err(char *msg, char *arg)
{
	write(STD_ERR, msg, ft_strlen(msg));
	if (arg)
	{
		write(STD_ERR, " ", 1);
		write(STD_ERR, arg, ft_strlen(arg));
	}
	write(STD_ERR, "\n", 1);
}

void clear_list(t_list *list)
{
	t_list *tmp;
	char	**str_p;
	
	while (list)
	{
		tmp = list;
		str_p = list->args;
		if (list->args)
			free(list->args);
		list = tmp->next;
		free(tmp);
	}
}

void exit_fatal()
{
	show_err(ERR_FATAL, NULL);
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

t_list *get_new_el(int cmds)
{
	t_list *new_el;

	new_el = malloc(sizeof(t_list));
	if (!new_el)
		exit_fatal();
	new_el->args = malloc((cmds + 1) * sizeof(char *));
	if (!new_el->args)
		exit_fatal();
	(new_el->args)[cmds] = NULL;
	new_el->type = TYPE_END;
	new_el->pipes[0] = -2;
	new_el->pipes[1] = -2;
	return (new_el);
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
		if (*list)
		{
			current_el = to_last_el(*list);
			current_el->type = (!strcmp(arg[inner_i], "|")) ? TYPE_PIPE : TYPE_END;
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
	current_el = get_new_el(cmds_in_block);
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
}

int cd_builtin(t_list *el)
{
	int	args_count;

	args_count = 0;
	while (el->args[args_count])
		args_count++;
	if (args_count != 2)
	{
		show_err(CD_ERR_ARG, NULL);
		return (EXIT_FAILURE);
	}
	if (chdir(el->args[1]))
	{
		show_err(CD_ERR_CHDIR, el->args[1]);
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
	{
		if (pipe(el->pipes) == -1)
			exit_fatal();
	}
	pid = fork();
	if (pid == -1)
		exit_fatal();
	if (pid == 0)
	{
		if (el->type == TYPE_PIPE)
			dup2(el->pipes[1], STD_OUT);
		if (input_fd != -1)
			dup2(input_fd, STD_IN);
		execve(el->args[0], el->args, env);
		show_err(EXEC_ERR, el->args[0]);
		if (el->type == TYPE_PIPE)
			close(el->pipes[1]);
		exit(1);
	}
	else
	{
		if (el->type == TYPE_PIPE)
			close(el->pipes[1]);
		waitpid(pid, &exit_status, 0);
		if (WIFEXITED(exit_status))
			last_exit_code = WEXITSTATUS(exit_status);
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
