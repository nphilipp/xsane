#include "plugin-intl.h"


GimpPDBStatusType
embed_profile (gint32 image_ID, guchar * profile_name)
{

  GimpParasite *parasite;
  GimpParasite *actualparasite;
  FILE *profile;
  struct stat statbuf;
  int i;

  /* get file size */
  if (stat (profile_name, &statbuf) == 0)
    {
      guchar *tmp_profile;
      gint32 profile_size;

      profile_size = statbuf.st_size;
      profile = fopen (profile_name, "r");
      if (profile != NULL)
	{
	  tmp_profile = malloc (profile_size);
	  if (tmp_profile == NULL)
	    {
	      fclose (profile);
	      return GIMP_PDB_EXECUTION_ERROR;
	    }
	  i = fread (tmp_profile, 1, profile_size, profile);
	  if (profile_size == i)
	    {

	      parasite = gimp_parasite_new ("icc-profile", 0,
					    profile_size, tmp_profile);
              actualparasite= gimp_image_parasite_find(image_ID,"icc-profile");
	      if(actualparasite)
	      {
	         if(!gimp_parasite_compare(parasite,actualparasite))
	         {
		    fprintf(stderr,"Work Space conversion not implemented yet\n");
		    gimp_image_parasite_detach (image_ID,"icc-profile");
	         }
		 else
		 {
		    fprintf(stderr,"Workspace profile already embedded\n");
		    gimp_parasite_free (parasite);
		    free (tmp_profile);
		    return GIMP_PDB_SUCCESS;
		 }
	      }
	      gimp_image_parasite_attach (image_ID, parasite);
	      gimp_parasite_free (parasite);
	      free (tmp_profile);
	    }
	  else
	    {
	      fprintf (stderr, "only read %d bytes instead of %d\n", i,
		       profile_size);
	    }
	  fclose (profile);
	}
      else
	{
	  fprintf (stderr, "error in open\n");
	}
    }
  else
    {
      fprintf (stderr, "stat error (%d)\n", errno);
    }


  return GIMP_PDB_SUCCESS;
}
