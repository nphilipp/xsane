#include "stdio.h"

#define MM_PER_INCH 25.4

main(int argc, char *argv[])
{
 int val_in;
 int val_out;
 char option[255];
 char *string = 0;
 char *filename = 0;
 FILE *file;
 int len = 0;

  if (argc != 2) /* error ? */
  {
    fprintf(stderr,"USAGE: %s old-xsane.rc >new-xsane.rc\n", argv[0]);
    return;
  }      

  filename = argv[1];

  file = fopen(filename, "r");
  if (file == 0) /* error ? */
  {
    fprintf(stderr,"Could not open %s for reading *** ABORTED ***\n", filename);
    return;
  }      

  while (!feof(file))
  {
    fgets(option, sizeof(option), file); /* get option name */
    option[strlen(option)-1] = 0; /* remove cr */

    len = strlen(option);

    if (len)
    {
      if (option[len-1] == 34)
      {
        option[len-1] = 0; /* remove " */
      }
    }
    string = option+1;  

    if ((!strcmp(string, "printer-width")) ||
        (!strcmp(string, "printer-height")) ||
        (!strcmp(string, "printer-left-offset")) ||
        (!strcmp(string, "printer-bottom-offset")))
    {
      printf("\"%s\"\n", string);
      fscanf(file, "%d\n", &val_in);  
      val_out = val_in * 65536 * MM_PER_INCH/72.0;
      printf("%d\n", val_out);
    }
    else
    {
      printf("\"%s\"\n", string);
      fgets(option, sizeof(option), file); /* get option name */
      option[strlen(option)-1] = 0; /* remove cr */
      printf("%s\n", option);
    }
  }
  fclose(file); 
}
