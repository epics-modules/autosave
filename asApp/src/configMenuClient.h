/* configMenuClient.h */
typedef void (*callbackFunc)(int status, void *puserPvt);
extern int fdbrestoreX(char *filename, char *macrostring, callbackFunc callbackFunction, void *puserPvt);
extern char *getMacroString(char *request_file);
extern int manual_save(char *request_file, char *save_file, callbackFunc callbackFunction, void *puserPvt);
extern int findConfigFiles(char *config, char names[][100], char descriptions[][100], int num, int len);

