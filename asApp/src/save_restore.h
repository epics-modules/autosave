/* save_restore.h */

/*	
 * 	Original Author: Frank Lenkszus
 *	Current Author:  Frank Lenkszus
 *	Date:		7/10/97
 *
 */

#define         MAX(a,b)   ((a)>(b)?(a):(b))
#define         MIN(a,b)   ((a)<(b)?(a):(b))
#define         MAXRESTOREFILES 8

struct restoreList {
        int pass0cnt;
        int pass1cnt;
        char *pass0files[MAXRESTOREFILES];
        char *pass1files[MAXRESTOREFILES];
};

FILE *fopen_and_check(const char *file, const char *mode);

