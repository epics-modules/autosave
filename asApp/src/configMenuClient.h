/* configMenuClient.h */
#define CONFIGMENU_ITEM_CHARS 40

typedef void (*callbackFunc)(int status, void *puserPvt);
extern int fdbrestoreX(char *filename, char *macrostring, callbackFunc callbackFunction, void *puserPvt);
extern char *getMacroString(char *request_file);
extern int manual_save(char *request_file, char *save_file, callbackFunc callbackFunction, void *puserPvt);
extern int findConfigFiles(char *config, ELLLIST *configMenuList);

struct configFileListItem {
    ELLNODE node;
	char *name;
	char *description;
};
