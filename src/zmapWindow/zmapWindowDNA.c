/*  File: zmapWindowDNA.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006: Genome Research Ltd.
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
 * originated by
 * 	Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk,
 *      Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk,
 *      Rob Clack (Sanger Institute, UK) rnc@sanger.ac.uk
 *
 * Description: dialog for searching for dna strings.
 *
 * Exported functions: See zmapWindow_P.h
 * HISTORY:
 * Last edited: Oct 11 10:32 2006 (edgrif)
 * Created: Fri Oct  6 16:00:11 2006 (edgrif)
 * CVS info:   $Id: zmapWindowDNA.c,v 1.2 2006-11-08 09:25:05 edgrif Exp $
 *-------------------------------------------------------------------
 */

#include <string.h>
#include <ZMap/zmapUtils.h>
#include <ZMap/zmapDNA.h>
#include <zmapWindow_P.h>


typedef struct
{
  ZMapWindow window ;

  ZMapFeatureBlock block ;

  GtkWidget *toplevel ;
  GtkWidget *dna_entry ;

  int search_start ;
  int search_end ;

  int max_errors ;
  int max_Ns ;

} DNASearchDataStruct, *DNASearchData ;



static void requestDestroyCB(gpointer data, guint callback_action, GtkWidget *widget) ;
static void destroyCB(GtkWidget *widget, gpointer cb_data) ;
static void helpCB(gpointer data, guint callback_action, GtkWidget *w) ;
static void searchCB(GtkWidget *widget, gpointer cb_data) ;
static void startSpinCB(GtkSpinButton *spinbutton, gpointer user_data) ;
static void endSpinCB(GtkSpinButton *spinbutton, gpointer user_data) ;
static void errorSpinCB(GtkSpinButton *spinbutton, gpointer user_data) ;
static void nSpinCB(GtkSpinButton *spinbutton, gpointer user_data) ;

static GtkWidget *makeMenuBar(DNASearchData search_data) ;

static GtkWidget *makeSpinPanel(DNASearchData search_data,
				char *title,
				char *spin_label, int min, int max, int init, GtkSignalFunc func,
				char *spin_label2, int min2, int max2, int init2, GtkSignalFunc func2) ;

static void remapCoords(gpointer data, gpointer user_data) ;
static void printCoords(gpointer data, gpointer user_data) ;



static GtkItemFactoryEntry menu_items_G[] = {
 { "/_File",           NULL,          NULL,          0, "<Branch>",      NULL},
 { "/File/Close",      "<control>W",  requestDestroyCB,    0, NULL,            NULL},
 { "/_Help",           NULL,          NULL,          0, "<LastBranch>",  NULL},
 { "/Help/Overview",   NULL,          helpCB,      0, NULL,            NULL}
};




void zmapWindowCreateDNAWindow(ZMapWindow window, FooCanvasItem *feature_item)
{
  GtkWidget *toplevel, *vbox, *menubar, *topbox, *hbox, *frame, *entry,
    *search_button, *start_end, *errors, *buttonBox ;
  DNASearchData search_data ;
  ZMapFeatureAny feature_any ;
  ZMapFeatureBlock block ;
  int max_errors, max_Ns ;

  /* Need to check that there is any dna...n.b. we need the item that was clicked for us to check
   * the dna..... */
  feature_any = g_object_get_data(G_OBJECT(feature_item), ITEM_FEATURE_DATA) ;
  zMapAssert(feature_any) ;
  block = (ZMapFeatureBlock)zMapFeatureGetParentGroup(feature_any,
						      ZMAPFEATURE_STRUCT_BLOCK) ;

  if (block->sequence.type == ZMAPSEQUENCE_NONE)
    {
      zMapWarning("Cannot search DNA in this block \"%s\", "
		  "either it has no DNA or the DNA was not fetched from the server.",
		  g_quark_to_string(block->original_id)) ;

      return ;
    }


  search_data = g_new0(DNASearchDataStruct, 1) ;

  search_data->window = window ;
  search_data->block = block ;
  search_data->search_start = block->block_to_sequence.q1 ;
  search_data->search_end = block->block_to_sequence.q2 ;

  /* Clamp to length of sequence, useless to do that but possible.... */
  max_errors = max_Ns = block->block_to_sequence.q2 - block->block_to_sequence.q1 + 1 ;


  /* set up the top level window */
  search_data->toplevel = toplevel = gtk_window_new(GTK_WINDOW_TOPLEVEL) ;
  g_signal_connect(GTK_OBJECT(toplevel), "destroy",
		   GTK_SIGNAL_FUNC(destroyCB), (gpointer)search_data) ;

  gtk_container_border_width(GTK_CONTAINER(toplevel), 5) ;
  gtk_window_set_title(GTK_WINDOW(toplevel), "DNA Search") ;
  gtk_window_set_default_size(GTK_WINDOW(toplevel), 500, -1) ;


  /* Add ptrs so parent knows about us */
  g_ptr_array_add(window->dna_windows, (gpointer)toplevel) ;


  vbox = gtk_vbox_new(FALSE, 0) ;
  gtk_container_add(GTK_CONTAINER(toplevel), vbox) ;

  menubar = makeMenuBar(search_data) ;
  gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);


  /* Make the box for dna text. */
  hbox = gtk_hbox_new(FALSE, 0) ;
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  frame = gtk_frame_new( "Enter DNA: " );
  gtk_frame_set_label_align(GTK_FRAME(frame), 0.0, 0.0 );
  gtk_container_border_width(GTK_CONTAINER(frame), 5);
  gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0) ;

  topbox = gtk_vbox_new(FALSE, 5) ;
  gtk_container_border_width(GTK_CONTAINER(topbox), 5) ;
  gtk_container_add (GTK_CONTAINER (frame), topbox) ;


  search_data->dna_entry = entry = gtk_entry_new() ;
  gtk_entry_set_activates_default (GTK_ENTRY(entry), TRUE);
  gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1) ;
  gtk_box_pack_start(GTK_BOX(topbox), entry, FALSE, FALSE, 0) ;

  
  /* Make the start/end boxes. */
  hbox = gtk_hbox_new(FALSE, 0) ;
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  start_end = makeSpinPanel(search_data,
			    "Set Start/End coords of search: ",
			    "Start :", search_data->search_start, search_data->search_end,
			    search_data->search_start, GTK_SIGNAL_FUNC(startSpinCB),
			    "End :", search_data->search_start, search_data->search_end,
			    search_data->search_end, GTK_SIGNAL_FUNC(endSpinCB)) ;
  gtk_box_pack_start(GTK_BOX(hbox), start_end, TRUE, TRUE, 0) ;


  /* Make the error boxes. */
  hbox = gtk_hbox_new(FALSE, 0) ;
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  errors = makeSpinPanel(search_data,
			 "Set Maximum Acceptable Error Rates: ",
			 "Mismatches :", 0, max_errors, 0, GTK_SIGNAL_FUNC(errorSpinCB),
			 "N bases :", 0, max_Ns, 0, GTK_SIGNAL_FUNC(nSpinCB)) ;
  gtk_box_pack_start(GTK_BOX(hbox), errors, TRUE, TRUE, 0) ;


  /* Make control buttons. */
  frame = gtk_frame_new("") ;
  gtk_container_set_border_width(GTK_CONTAINER(frame), 
                                 ZMAP_WINDOW_GTK_CONTAINER_BORDER_WIDTH);
  gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0) ;


  buttonBox = gtk_hbutton_box_new();
  gtk_container_add(GTK_CONTAINER(frame), buttonBox);
  gtk_button_box_set_layout (GTK_BUTTON_BOX (buttonBox), GTK_BUTTONBOX_END);
  gtk_box_set_spacing (GTK_BOX(buttonBox), 
                       ZMAP_WINDOW_GTK_BUTTON_BOX_SPACING);
  gtk_container_set_border_width (GTK_CONTAINER (buttonBox), 
                                  ZMAP_WINDOW_GTK_CONTAINER_BORDER_WIDTH);


  search_button = gtk_button_new_with_label("Search") ;
  gtk_container_add(GTK_CONTAINER(buttonBox), search_button) ;
  gtk_signal_connect(GTK_OBJECT(search_button), "clicked",
		     GTK_SIGNAL_FUNC(searchCB), (gpointer)search_data) ;
  /* set search button as default. */
  GTK_WIDGET_SET_FLAGS(search_button, GTK_CAN_DEFAULT) ;
  gtk_window_set_default(GTK_WINDOW(toplevel), search_button) ;


  gtk_widget_show_all(toplevel) ;

  return ;
}




/*
 *                 Internal functions
 */


GtkWidget *makeMenuBar(DNASearchData search_data)
{
  GtkAccelGroup *accel_group;
  GtkItemFactory *item_factory;
  GtkWidget *menubar ;
  gint nmenu_items = sizeof (menu_items_G) / sizeof (menu_items_G[0]);

  accel_group = gtk_accel_group_new ();

  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group );

  gtk_item_factory_create_items(item_factory, nmenu_items, menu_items_G, (gpointer)search_data) ;

  gtk_window_add_accel_group(GTK_WINDOW(search_data->toplevel), accel_group) ;

  menubar = gtk_item_factory_get_widget(item_factory, "<main>");

  return menubar ;
}



static GtkWidget *makeSpinPanel(DNASearchData search_data,
				char *title,
				char *spin_label, int min, int max, int init, GtkSignalFunc func,
				char *spin_label2, int min2, int max2, int init2, GtkSignalFunc func2)
{
  GtkWidget *frame ;
  GtkWidget *topbox, *hbox, *label, *error_spinbox, *n_spinbox ;


  frame = gtk_frame_new(title);
  gtk_frame_set_label_align( GTK_FRAME( frame ), 0.0, 0.0 );
  gtk_container_border_width(GTK_CONTAINER(frame), 5);

  topbox = gtk_vbox_new(FALSE, 5) ;
  gtk_container_border_width(GTK_CONTAINER(topbox), 5) ;
  gtk_container_add (GTK_CONTAINER (frame), topbox) ;

  hbox = gtk_hbox_new(FALSE, 0) ;
  gtk_container_border_width(GTK_CONTAINER(hbox), 0);
  gtk_box_pack_start(GTK_BOX(topbox), hbox, TRUE, FALSE, 0) ;

  label = gtk_label_new(spin_label) ;
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT) ;
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0) ;

  error_spinbox =  gtk_spin_button_new_with_range(min, max, 1.0) ;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(error_spinbox), init) ;
  gtk_signal_connect(GTK_OBJECT(error_spinbox), "value-changed",
		     GTK_SIGNAL_FUNC(func), (gpointer)search_data) ;
  gtk_box_pack_start(GTK_BOX(hbox), error_spinbox, FALSE, FALSE, 0) ;

  label = gtk_label_new(spin_label2) ;
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT) ;
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0) ;

  n_spinbox =  gtk_spin_button_new_with_range(min2, max2, 1.0) ;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(n_spinbox), init2) ;
  gtk_signal_connect(GTK_OBJECT(n_spinbox), "value-changed",
		     GTK_SIGNAL_FUNC(func2), (gpointer)search_data) ;
  gtk_box_pack_start(GTK_BOX(hbox), n_spinbox, FALSE, FALSE, 0) ;


  return frame ;
}


static void searchCB(GtkWidget *widget, gpointer cb_data)
{
  DNASearchData search_data = (DNASearchData)cb_data ;
  char *query_txt = NULL ;
  char *err_text = NULL ;
  char *dna ;
  int start, end, dna_len ;


  /* NEED TO SORT WHOLE COORD JUNK OUT....USER SHOULD SEE BLOCK COORDS WE SHOULD DO RELATIVE COORDS... */
  /* Convert to relative coords.... */
  start = search_data->search_start - search_data->block->block_to_sequence.q1 ;
  end = search_data->search_end - search_data->block->block_to_sequence.q1 ;
  dna = search_data->block->sequence.sequence ;
  dna_len = strlen(dna) ;


  /* Note that gtk_entry returns "" for no text, _not_ NULL. */
  query_txt = (char *)gtk_entry_get_text(GTK_ENTRY(search_data->dna_entry)) ;
  query_txt = g_strdup(query_txt) ;
  query_txt = g_strstrip(query_txt) ;

  if (strlen(query_txt) == 0)
    err_text = g_strdup("no query dna supplied.") ;
  else if (!zMapDNAValidate(query_txt))
    err_text = g_strdup("query dna contains invalid bases.") ;
  else if ((start < 0 || start > dna_len)
	   || (end < 0 || end > dna_len)
	   || (search_data->max_errors < 0 || search_data->max_errors > dna_len)
	   || (search_data->max_Ns < 0 || search_data->max_Ns > dna_len))
    err_text = g_strdup_printf("start/end/max errors/max Ns must all be within range %d -> %d",
			       search_data->block->block_to_sequence.q1,
			       search_data->block->block_to_sequence.q2) ;

  if (err_text)
    {
      zMapMessage("Please correct and retry: %s", err_text) ;
    }
  else
    {
      GList *match_list ;

      if ((match_list = zMapDNAFindAllMatches(dna, query_txt, start, end,
					      search_data->max_errors, search_data->max_Ns, TRUE)))
	{
	  char *title ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
	  g_list_foreach(match_list, printCoords, dna) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

	  title = g_strdup_printf("Matches for \"%s\", (start = %d, end = %d, max errors = %d, max N's %d",
				  g_quark_to_string(search_data->block->original_id),
				  search_data->search_start, search_data->search_end,
				  search_data->max_errors, search_data->max_Ns) ;


	  /* Need to convert coords back to block coords here.... */
	  g_list_foreach(match_list, remapCoords, search_data->block) ;

	  zmapWindowDNAListCreate(search_data->window, match_list, title, search_data->block) ;

	  g_free(title) ;
	}
      else
	{
	  zMapMessage("Sorry, no matches in sequence \"%s\" for query \"%s\"",
		      g_quark_to_string(search_data->block->original_id), query_txt) ;
	}
    }

  if (err_text)
    g_free(err_text) ;
  g_free(query_txt) ;

  return ;
}



/* This is not the way to do help, we should really used html and have a set of help files. */
static void helpCB(gpointer data, guint callback_action, GtkWidget *w)
{
  char *title = "DNA Search Window" ;
  char *help_text =
    "The ZMap DNA Search Window allows you to search for DNA. You can cut/paste DNA into the\n"
    "DNA text field or type it in. You can specify the maximum number of mismatches\n"
    "and the maximum number of ambiguous/unknown bases acceptable in the match.\n" ;

  zMapGUIShowText(title, help_text, FALSE) ;

  return ;
}


/* Requests destroy of search window which will end up with gtk calling destroyCB(). */
static void requestDestroyCB(gpointer cb_data, guint callback_action, GtkWidget *widget)
{
  DNASearchData search_data = (DNASearchData)cb_data ;

  gtk_widget_destroy(search_data->toplevel) ;

  return ;
}


static void destroyCB(GtkWidget *widget, gpointer cb_data)
{
  DNASearchData search_data = (DNASearchData)cb_data ;

  g_ptr_array_remove(search_data->window->dna_windows, (gpointer)search_data->toplevel);

  return ;
}


static void startSpinCB(GtkSpinButton *spin_button, gpointer user_data)
{
  DNASearchData search_data = (DNASearchData)user_data ;

  search_data->search_start = gtk_spin_button_get_value_as_int(spin_button) ;

  return ;
}

static void endSpinCB(GtkSpinButton *spin_button, gpointer user_data)
{
  DNASearchData search_data = (DNASearchData)user_data ;

  search_data->search_end = gtk_spin_button_get_value_as_int(spin_button) ;

  return ;
}


static void errorSpinCB(GtkSpinButton *spin_button, gpointer user_data)
{
  DNASearchData search_data = (DNASearchData)user_data ;

  search_data->max_errors = gtk_spin_button_get_value_as_int(spin_button) ;

  return ;
}

static void nSpinCB(GtkSpinButton *spin_button, gpointer user_data)
{
  DNASearchData search_data = (DNASearchData)user_data ;

  search_data->max_Ns = gtk_spin_button_get_value_as_int(spin_button) ;

  return ;
}



static void remapCoords(gpointer data, gpointer user_data)
{
  ZMapDNAMatch match_data = (ZMapDNAMatch)data ;
  ZMapFeatureBlock block = (ZMapFeatureBlock)user_data ;

  match_data->start = match_data->start + block->block_to_sequence.q1 ;
  match_data->end = match_data->end + block->block_to_sequence.q1 ;

  return ;
}


static void printCoords(gpointer data, gpointer user_data)
{
  ZMapDNAMatch match_data = (ZMapDNAMatch)data ;
  char *dna = (char *)user_data ;
  GString *match_str ;

  match_str = g_string_new("") ;

  match_str = g_string_append_len(match_str, (dna + match_data->start),
				  (match_data->end - match_data->start + 1)) ;

  printf("Start, End  =  %d, %d     %s\n", match_data->start, match_data->end, match_str->str) ;


  g_string_free(match_str, TRUE) ;

  return ;
}
