#include "kopou.h"

int execute_command(kconnection_t *c, kcommand_t *cmd)
{
	return cmd->execute(c, cmd);
}

int bucket_put_cmd(kconnection_t *c, kcommand_t *cmd)
{
	K_FORCE_USE(cmd);
	reply_201(c);
	return K_OK;
}

int bucket_head_cmd(kconnection_t *c, kcommand_t *cmd)
{
	K_FORCE_USE(cmd);
	reply_200(c);
	return K_OK;
}

int bucket_get_cmd(kconnection_t *c, kcommand_t *cmd)
{
	K_FORCE_USE(cmd);
	reply_200(c);
	return K_OK;
}

int bucket_delete_cmd(kconnection_t *c, kcommand_t *cmd)
{
	K_FORCE_USE(cmd);
	reply_200(c);
	return K_OK;
}


