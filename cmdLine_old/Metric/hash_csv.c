#include <stdio.h>
#include <string.h>
#include <glib.h>

#define MAX_STR 1000

GHashTable *hTable = NULL;

int load_csv(char *file)
{
	int i = 0, j = 0;
	
	FILE *pFile;
	char* tmpCsv;
	char  line[MAX_STR];

	int a[3];
	int assigned;
	char operation[20];
	char *key, *val;
	
	char tmpKey[MAX_STR], tmpVal[100];

	char * pch;
	char *hPtr;

	if(hTable == NULL)
		hTable = g_hash_table_new_full(g_str_hash,g_str_equal,g_free,g_free);	
	 pFile = fopen(file,"r");
	 
	 if (pFile == NULL)
	 { 
		 perror ("Error opening file");
		 return -1;
	 }
		 
	 while ( fgets (line , MAX_STR , pFile) != NULL )
	 {

//		 puts (line);
		 if(line[0] != '"')
		 {
			memset(operation,0,sizeof(operation));
			memset(&a,0,3 * sizeof(int));
			
			assigned = sscanf(line,"%d,%d,%s\n",&a[0],&a[1],operation);

			pch=strchr(operation,',');
			
			if(pch == NULL)
				printf("no , found\n");
			else  
			{
				sscanf(pch+1,"%d",&a[2]);
			}
			  
			if (pch == operation)
				memset(operation,0,sizeof(operation));
			else
				operation[(pch - operation)] = '\0';

//			printf("%d,%d,%s,%d\n", a[0], a[1],operation, a[2]);
	
			sprintf(tmpKey, "%s,%d", file,a[0]);
//			printf("=== > %s\n",tmpKey);
			
//				sprintf(tmpVal, "%d,%s,%d", a[1],operation, a[2]);
			sprintf(tmpVal, "%d,%s", a[1],operation);
//			printf("=== > %s\n",tmpVal);

			val = g_hash_table_lookup(hTable,tmpKey);

			if(val == NULL)
				g_hash_table_insert(hTable,g_strdup(tmpKey),g_strdup(tmpVal));
		 }

	 }
	 
	fclose(pFile);
		 
	return 0;
}

static void hash_table_print(gpointer key,gpointer value,gpointer user_data)
{
	char *hKey = (char *)key;
	char *hVal = (char *)value;

	printf("%s:%s\n",hKey,hVal);

}

char* lookup_csv(char* key)
{
	char *val = g_hash_table_lookup(hTable,key);
	return val;

}

void destroy_csv(void)
{
	g_hash_table_destroy(hTable);
	hTable = NULL;
}

#if 0
main()
{
//	char *files[] = {"/home/bray/repertoire/conv_new/FreeBSD.csv"};
	char *files[] = {"temp.csv"};
	load_csv(files,1);

	g_hash_table_foreach(hTable,(GHFunc)hash_table_print,NULL);

	printf("lookup value = %s\n",lookup_csv("temp.csv,4"));

	g_hash_table_destroy(hTable);

}
#endif
