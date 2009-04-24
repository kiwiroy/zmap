/*  File: zmapUtilsUsers.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2008: Genome Research Ltd.
 *-------------------------------------------------------------------
 * ZMap is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 * or see the on-line version at http://www.gnu.org/copyleft/gpl.txt
 *-------------------------------------------------------------------
 * This file is part of the ZMap genome database package
 * originally written by:
 *
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *
 * Description: Contains routines for various handling of users.
 *
 * Exported functions: See zmapUtils.h
 * HISTORY:
 * Last edited: Apr 24 11:23 2009 (rds)
 * Created: Fri Dec 12 13:14:55 2008 (edgrif)
 * CVS info:   $Id: zmapUtilsUsers.c,v 1.2 2009-04-24 10:30:39 rds Exp $
 *-------------------------------------------------------------------
 */

#include <string.h>
#include <sys/utsname.h>
#include <netdb.h>

#include <zmapUtils_P.h>



/* This is all kept as static because after all there is just one user at a time.... */

/* If true then various extra menu options and functions are turned on. */
static gboolean developer_status_G = FALSE ;


/* Currently developers are limited to certain ids in certain domains. */
static char *developers_G[] = {"edgrif", "rds", NULL} ;
static char *domain_G[] = {"sanger.ac.uk", NULL} ;


/* GLib 2.16 has the GChecksum package which we could use to encrypt this, otherwise we need
 * to include another library... */
static char *passwd_G = "rubbish" ;



/* 
 *               Interface functions
 */


/*! @addtogroup zmaputils
 * @{
 * 
 * \brief  Set of utilities for user-related functions.
 * 
 *  */




/*!
 * Initialises user package with current user/host/domain name information.
 * Must be called before other zMapUserXXX() functions otherwise
 * their results are undefined.
 *
 * @param   void  None.
 * @return        void
 *  */
void zMapUtilsUserInit(void)
{
  char *real_name, *user_name ;
  char *host_name, *domain_name ;
  struct hostent *domain_data ;
  int result ;
  struct utsname name ;
  gboolean status = FALSE ;
  gboolean name_status, domain_status;

  result      = uname(&name) ;

  user_name   = (char *)g_get_user_name() ;
  real_name   = (char *)g_get_real_name() ;

  host_name   = (char *)g_get_host_name() ;
  domain_data = gethostbyname(host_name) ;
  domain_name = domain_data->h_name ;

  name_status = domain_status = FALSE;

  /* Is the current user a developer ? */
  if (result == 0)
    {
      char **curr_user ;

      curr_user = &developers_G[0] ;
      while (curr_user && *curr_user)
	{
	  if (strcmp(user_name, *curr_user) == 0)
	    {
	      name_status = TRUE ;

	      break ;
	    }

	  curr_user++ ;
	}
    }

  /* Is this a supported domain ? */
  if (name_status)
    {
      char **curr_domain ;

      curr_domain = &domain_G[0] ;
      while (curr_domain && *curr_domain)
	{
	  if (g_str_has_suffix(domain_name, *curr_domain))
	    {
	      domain_status = TRUE ;

	      break ;
	    }

	  curr_domain++ ;
	}
    }

  /* Currently developers are limited to certain ids in certain domains. */
  if(name_status && domain_status)
    status = TRUE;

  developer_status_G = status ;

  return ;
}


/*!
 * Tests whether current user has developer status.
 *
 * @param   void
 * @return  gboolean  TRUE if user has developer status.
 *  */
gboolean zMapUtilsUserIsDeveloper(void)
{
  return developer_status_G ;
}


/*!
 * Sets current user to have developer status for this process.
 * Fails if password is not correct.
 *
 * @param   void
 * @return  gboolean  TRUE if developer status set, FALSE otherwise.
 *  */
gboolean zMapUtilsUserSetDeveloper(char *passwd)
{
  gboolean result = FALSE ;

  if (strcmp(passwd, passwd_G) == 0)
    result = developer_status_G = TRUE ;

  return result ;
}





/*! @} end of zmaputils docs. */








/* 
 *               Internal functions
 */

