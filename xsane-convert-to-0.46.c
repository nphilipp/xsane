#include "stdio.h"

main(int argc, char *argv[])
{
 double val_double;
 long int val_sane;
 char option[255];
 char *string = 0;
 char *filename = 0;
 FILE *file;
 int len = 0;

  if (argc != 2) /* error ? */
  {
    fprintf(stderr,"USAGE: %s file.drc >outputfile.drc\n", argv[0]);
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

    if ((!strcmp(string, "xsane-main-window-x-position")) ||
        (!strcmp(string, "xsane-main-window-y-position")) ||
        (!strcmp(string, "xsane-main-window-width")) ||
        (!strcmp(string, "xsane-main-window-height")) ||
        (!strcmp(string, "xsane-standard-options-window-x-position")) ||
        (!strcmp(string, "xsane-standard-options-window-y-position")) ||
        (!strcmp(string, "xsane-advanced-options-window-x-position")) ||
        (!strcmp(string, "xsane-advanced-options-window-y-position")) ||
        (!strcmp(string, "xsane-histogram-window-x-position")) ||
        (!strcmp(string, "xsane-histogram-window-y-position")) ||
        (!strcmp(string, "xsane-preview-window-x-position")) ||
        (!strcmp(string, "xsane-preview-window-y-position")) ||
        (!strcmp(string, "xsane-preview-window-width")) ||
        (!strcmp(string, "xsane-preview-window-height")) ||
        (!strcmp(string, "xsane-gamma")) ||
        (!strcmp(string, "xsane-gamma-red")) ||
        (!strcmp(string, "xsane-gamma-green")) ||
        (!strcmp(string, "xsane-gamma-blue")) ||
        (!strcmp(string, "xsane-brightness")) ||
        (!strcmp(string, "xsane-brightness-red")) ||
        (!strcmp(string, "xsane-brightness-green")) ||
        (!strcmp(string, "xsane-brightness-blue")) ||
        (!strcmp(string, "xsane-contrast")) ||
        (!strcmp(string, "xsane-contrast-red")) ||
        (!strcmp(string, "xsane-contrast-green")) ||
        (!strcmp(string, "xsane-contrast-blue")))
    {
      printf("\"%s\"\n", string);
      fscanf(file, "%lf\n", &val_double);  
      val_sane = val_double * 65536;
      printf("%d\n", val_sane);
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
