#pragma once

#define SHELL_PROMPT "kernel_shell >> "
#define CMD_BUFFER_MAX 256
#define CMD_TOKS_MAX 64

int ls_cmd(int argc, char argv[][CMD_BUFFER_MAX]);
int mkdir_cmd(int argc, char argv[][CMD_BUFFER_MAX]);
int rm_cmd(int argc, char argv[][CMD_BUFFER_MAX]);

int readfile_cmd(int argc, char argv[][CMD_BUFFER_MAX]);

int cd_cmd(int argc, char argv[][CMD_BUFFER_MAX]);
int pwd_cmd(int argc, char argv[][CMD_BUFFER_MAX]);

int lspci_cmd(int argc, char argv[][CMD_BUFFER_MAX]);