#ifndef CVAR_H
#define CVAR_H

 // =========================================================
 // Cvar structure
 // =========================================================

typedef struct cvar_s
{
	char			*name;
	char			*string;
	qboolean		archive; // Saved to config.cfg
	qboolean		server; // Sent to clients as serverinfo
	float			value;
	struct cvar_s	*next;
} cvar_t;

 // =========================================================
 // Prototypes
 // =========================================================

void Cvar_Init(void);
void Cvar_RegisterVariable(cvar_t *variable);
void Cvar_Set(const char *var_name, const char *value);
void Cvar_SetValue(const char *var_name, float value);
float Cvar_VariableValue(const char *var_name);
char *Cvar_VariableString(const char *var_name);
cvar_t *Cvar_FindVar(const char *var_name);
int  Cvar_Command(void);
void Cvar_WriteVariables(FILE *f);

char *Cvar_CompleteVariable(const char *partial);

#endif // CVAR_H

