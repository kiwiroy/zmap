/*  File: zmapWindowDNAChoose.c
 *  Author: Ed Griffiths (edgrif@sanger.ac.uk)
 *  Copyright (c) 2006-2017: Genome Research Ltd.
 *-------------------------------------------------------------------
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *-------------------------------------------------------------------
 * This file is part of the ZMap genome database package
 * originally written by:
 * 
 *      Ed Griffiths (Sanger Institute, UK) edgrif@sanger.ac.uk
 *        Roy Storey (Sanger Institute, UK) rds@sanger.ac.uk
 *   Malcolm Hinsley (Sanger Institute, UK) mh17@sanger.ac.uk
 *       Gemma Guest (Sanger Institute, UK) gb10@sanger.ac.uk
 *      Steve Miller (Sanger Institute, UK) sm23@sanger.ac.uk
 *  
 * Description: Shows a dialog window allowing user to choose a section
 *              of DNA to export. Initially the DNA section is that
 *              of the feature the user selected and they can add
 *              a +/- extra section to that. The extent of the dna to
 *              be exported is shown as an overlay box over the feature.
 *
 * Exported functions: See zmapWindow_P.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>


#include <string.h>
#include <gdk/gdkkeysyms.h>
#include <ZMap/zmapUtils.hpp>
#include <ZMap/zmapDNA.hpp>
#include <zmapWindow_P.hpp>
#include <zmapWindowContainerUtils.hpp>

typedef struct
{
  ZMapWindow window ;

  ZMapFeatureBlock block ;

  FooCanvasItem *feature_item;
  FooCanvasItem *seq_item;

  GtkWidget *toplevel ;
  GtkWidget *dna_entry ;
  GtkSpinButton *start_spin ;
  GtkSpinButton *end_spin ;
  GtkSpinButton *flanking_spin ;

#if OVERLAY
  double item_x1, item_y1, item_x2, item_y2 ;
  FooCanvasItem *overlay_box ;
#endif

  gboolean enter_pressed ;

  char *entry_text ;

  int dna_start ;
  int dna_end ;
  int dna_flanking ;
  int sequence_start ;
  int sequence_end ;

  gboolean revcomp ;

  char *dna ;
} DNASearchDataStruct, *DNASearchData ;



static void requestDestroyCB(gpointer data, guint callback_action, GtkWidget *widget) ;
static void destroyCB(GtkWidget *widget, gpointer cb_data) ;
static void helpCB(gpointer data, guint callback_action, GtkWidget *w) ;
static void startSpinCB(GtkSpinButton *spinbutton, gpointer user_data) ;
static void endSpinCB(GtkSpinButton *spinbutton, gpointer user_data) ;
static void flankingSpinCB(GtkSpinButton *spinbutton, gpointer user_data) ;

static gboolean spinEventCB(GtkWidget *widget, GdkEventClient *event, gpointer data) ;
static void updateSpinners(DNASearchData dna_data) ;

static GtkWidget *makeMenuBar(DNASearchData dna_data) ;
static GtkWidget *makeSpinPanel(DNASearchData dna_data,
                                const char *title,
                                const char *spin_label, int min_val, int max_val, int init, GtkSignalFunc func,
                                const char *spin_label2, int min2, int max2, int init2, GtkSignalFunc func2,
                                const char *spin_label3, int min3, int max3, int init3, GtkSignalFunc func3) ;
static void getDNA(DNASearchData dna_data) ;




static GtkItemFactoryEntry menu_items_G[] = {
 { (gchar *)"/_File",           NULL,          NULL,          0, (gchar *)"<Branch>",      NULL},
 { (gchar *)"/File/Close",      (gchar *)"<control>W",  (GtkItemFactoryCallback)requestDestroyCB,    0, NULL,            NULL},
 { (gchar *)"/_Help",           NULL,          NULL,          0, (gchar *)"<LastBranch>",  NULL},
 { (gchar *)"/Help/Overview",   NULL,          (GtkItemFactoryCallback)helpCB,      0, NULL,            NULL}
};




/* Returns the DNA chosen by the user with the start/end coords. */
char *zmapWindowDNAChoose(ZMapWindow window, FooCanvasItem *feature_item, ZMapWindowDialogType dialog_type,
                          int *sequence_start_out, int *sequence_end_out)
{
  char *dna = NULL ;
  GtkWidget *toplevel, *vbox, *menubar, *topbox, *hbox, *frame, *entry, *start_end ;
  GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT ;
  DNASearchData dna_data ;
  ZMapFeature feature ;
  ZMapFeatureBlock block ;
  double x1, y1, x2, y2 ;
  FooCanvasItem *parent ;
  gint block_start, block_end ;
  const char *button_text ;
  gint dialog_result ;


  /* Need to check that there is any dna...n.b. we need the item that was clicked for us to check
   * the dna..... */
  feature = zmapWindowItemGetFeature(feature_item);
  if (!feature)
    return dna ;
  block = (ZMapFeatureBlock)zMapFeatureGetParentGroup((ZMapFeatureAny)feature, ZMAPFEATURE_STRUCT_BLOCK) ;

  if (block->sequence.type == ZMAPSEQUENCE_NONE)
    {
      zMapWarning("Cannot search DNA in this block \"%s\", "
                  "either it has no DNA or the DNA was not fetched from the server.",
                  g_quark_to_string(block->original_id)) ;

      return dna ;
    }

  block_start = block->block_to_sequence.block.x1 ;
  block_end   = block->block_to_sequence.block.x2 ;

  dna_data = g_new0(DNASearchDataStruct, 1) ;
  dna_data->window = window ;
  dna_data->block = block ;
  dna_data->feature_item = feature_item;
  dna_data->seq_item = zmapWindowItemGetDNATextItem(window,feature_item);
  dna_data->dna_start = feature->x1 ;
  dna_data->dna_end   = feature->x2 ;
  dna_data->dna_flanking = 0 ;
  if (feature->strand == ZMAPSTRAND_REVERSE)
    dna_data->revcomp = TRUE ;
  else
    dna_data->revcomp = FALSE ;

  zmapWindowCoordPairToDisplay(window, window->display_coordinates,
                               block_start, block_end, &block_start, &block_end) ;
  zmapWindowCoordPairToDisplay(window, window->display_coordinates,
                               dna_data->dna_start, dna_data->dna_end,
                               &(dna_data->dna_start), &(dna_data->dna_end)) ;


  /* Draw an overlay box over the feature to show the extent of the dna selected. */
  parent = zmapWindowItemGetTrueItem(feature_item) ;
  foo_canvas_item_get_bounds(parent, &x1, &y1, &x2, &y2) ;

#if OVERLAY
  GdkColor overlay_colour ;

  gdk_color_parse("red", &(overlay_colour)) ;
  dna_data->item_x1 = x1 ;
  dna_data->item_y1 = y1 ;
  dna_data->item_x2 = x2 ;
  dna_data->item_y2 = y2 ;
#endif

  zmapWindowHighlightSequenceItem(window, dna_data->seq_item,
                                    block->block_to_sequence.block.x1 + dna_data->dna_start,
                                    block->block_to_sequence.block.x1 + dna_data->dna_end, dna_data->dna_flanking);


  /* set up the top level window */
  button_text = zmapWindowGetDialogText(dialog_type) ;
  dna_data->toplevel = toplevel = gtk_dialog_new_with_buttons("Choose DNA Extent", NULL, flags,
                                                              button_text, GTK_RESPONSE_ACCEPT,
                                                              "Cancel", GTK_RESPONSE_NONE,
                                                              NULL) ;
  g_signal_connect(GTK_OBJECT(toplevel), "destroy",
                   GTK_SIGNAL_FUNC(destroyCB), (gpointer)dna_data) ;
  gtk_container_border_width(GTK_CONTAINER(toplevel), 5) ;
  gtk_window_set_default_size(GTK_WINDOW(toplevel), 500, -1) ;


  /* Add ptrs so parent knows about us */
  g_ptr_array_add(window->dna_windows, (gpointer)toplevel) ;


  vbox = gtk_vbox_new(FALSE, 0) ;
  gtk_container_add(GTK_CONTAINER(GTK_BOX(GTK_DIALOG(toplevel)->vbox)), vbox) ;

  menubar = makeMenuBar(dna_data) ;
  gtk_box_pack_start(GTK_BOX(vbox), menubar, FALSE, FALSE, 0);


  /* Make the box for dna text. */
  hbox = gtk_hbox_new(FALSE, 0) ;
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

  frame = gtk_frame_new( "Selected Feature: " );
  gtk_frame_set_label_align(GTK_FRAME(frame), 0.0, 0.0 );
  gtk_container_border_width(GTK_CONTAINER(frame), 5);
  gtk_container_set_focus_chain (GTK_CONTAINER(frame), NULL);
  gtk_box_pack_start(GTK_BOX(hbox), frame, TRUE, TRUE, 0) ;

  topbox = gtk_vbox_new(FALSE, 5) ;
  gtk_container_border_width(GTK_CONTAINER(topbox), 5) ;
  gtk_container_set_focus_chain (GTK_CONTAINER(topbox), NULL);
  gtk_container_add (GTK_CONTAINER (frame), topbox) ;


  dna_data->dna_entry = entry = gtk_entry_new() ;
  gtk_entry_set_activates_default (GTK_ENTRY(entry), TRUE) ;
  gtk_entry_set_text(GTK_ENTRY(entry), g_quark_to_string(feature->original_id)) ;
  gtk_box_pack_start(GTK_BOX(topbox), entry, FALSE, FALSE, 0) ;


  /* Make the start/end boxes. */
  hbox = gtk_hbox_new(FALSE, 0) ;
  gtk_container_set_focus_chain (GTK_CONTAINER(hbox), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  start_end = makeSpinPanel(dna_data,
                            "Set Start/End coords for DNA export: ",
                            "Start: ", block_start, dna_data->dna_end,
                            dna_data->dna_start, GTK_SIGNAL_FUNC(startSpinCB),
                            "End: ", dna_data->dna_start, block_end,
                            dna_data->dna_end, GTK_SIGNAL_FUNC(endSpinCB),
                            "Flanking bases: ", 0, (block->block_to_sequence.block.x2 - block->block_to_sequence.block.x1),
                            dna_data->dna_flanking, GTK_SIGNAL_FUNC(flankingSpinCB)) ;
  gtk_box_pack_start(GTK_BOX(hbox), start_end, TRUE, TRUE, 0) ;

  gtk_widget_show_all(toplevel) ;

  /* Run dialog in non-blocking mode so user can interact with the zmap window and even open
   * another dna window. */
  dialog_result = my_gtk_run_dialog_nonmodal(toplevel) ;

  /* Now get the dna and return it, note we must get entry text before destroying widgets. */
  if (dialog_result == GTK_RESPONSE_ACCEPT)
    {
      dna_data->entry_text = (char *)gtk_entry_get_text(GTK_ENTRY(dna_data->dna_entry)) ;

      getDNA(dna_data) ;

      /* Return results. */
      dna = dna_data->dna ;
      *sequence_start_out = dna_data->sequence_start ;
      *sequence_end_out = dna_data->sequence_end ;
    }

  /* and finally clear up ! */
  gtk_widget_destroy(toplevel) ;

  g_ptr_array_remove(window->dna_windows, (gpointer)toplevel) ;

  return dna ;
}




/*
 *                 Internal functions
 */


static GtkWidget *makeMenuBar(DNASearchData dna_data)
{
  GtkAccelGroup *accel_group;
  GtkItemFactory *item_factory;
  GtkWidget *menubar ;
  gint nmenu_items = sizeof (menu_items_G) / sizeof (menu_items_G[0]);

  accel_group = gtk_accel_group_new ();

  item_factory = gtk_item_factory_new (GTK_TYPE_MENU_BAR, "<main>", accel_group );

  gtk_item_factory_create_items(item_factory, nmenu_items, menu_items_G, (gpointer)dna_data) ;

  gtk_window_add_accel_group(GTK_WINDOW(dna_data->toplevel), accel_group) ;

  menubar = gtk_item_factory_get_widget(item_factory, "<main>");

  return menubar ;
}



static GtkWidget *makeSpinPanel(DNASearchData dna_data,
                                const  char *title,
                                const char *spin_label, int min_val, int max_val, int init, GtkSignalFunc func,
                                const char *spin_label2, int min2, int max2, int init2, GtkSignalFunc func2,
                                const char *spin_label3, int min3, int max3, int init3, GtkSignalFunc func3)
{
  GtkWidget *frame ;
  GtkWidget *topbox, *hbox, *label, *start_spinbox, *end_spinbox, *flanking_spinbox ;

  frame = gtk_frame_new(title);
  gtk_frame_set_label_align( GTK_FRAME( frame ), 0.0, 0.0 );
  gtk_container_border_width(GTK_CONTAINER(frame), 5);

  topbox = gtk_vbox_new(FALSE, 5) ;
  gtk_container_border_width(GTK_CONTAINER(topbox), 5) ;
  gtk_container_add (GTK_CONTAINER (frame), topbox) ;

  hbox = gtk_hbox_new(FALSE, 0) ;
  gtk_container_border_width(GTK_CONTAINER(hbox), 5);
  gtk_box_pack_start(GTK_BOX(topbox), hbox, TRUE, FALSE, 0) ;

  label = gtk_label_new(spin_label) ;
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT) ;
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0) ;

  start_spinbox =  gtk_spin_button_new_with_range(min_val, max_val, 1.0) ;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(start_spinbox), init) ;
  gtk_signal_connect(GTK_OBJECT(start_spinbox), "value-changed",
                     GTK_SIGNAL_FUNC(func), (gpointer)dna_data) ;
  gtk_box_pack_start(GTK_BOX(hbox), start_spinbox, FALSE, FALSE, 0) ;

  label = gtk_label_new(spin_label2) ;
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT) ;
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0) ;

  end_spinbox =  gtk_spin_button_new_with_range(min2, max2, 1.0) ;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(end_spinbox), init2) ;
  gtk_signal_connect(GTK_OBJECT(end_spinbox), "value-changed",
                     GTK_SIGNAL_FUNC(func2), (gpointer)dna_data) ;
  gtk_box_pack_start(GTK_BOX(hbox), end_spinbox, FALSE, FALSE, 0) ;

  label = gtk_label_new(spin_label3) ;
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT) ;
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, TRUE, 0) ;

  flanking_spinbox = gtk_spin_button_new_with_range(min3, max3, 1.0) ;
  gtk_spin_button_set_value(GTK_SPIN_BUTTON(flanking_spinbox), init3) ;
  g_signal_connect(GTK_OBJECT(flanking_spinbox), "value-changed",
                   G_CALLBACK(func3), (gpointer)dna_data) ;
  gtk_box_pack_start(GTK_BOX(hbox), flanking_spinbox, FALSE, FALSE, 0) ;



  g_signal_connect(GTK_OBJECT(start_spinbox), "event",
   GTK_SIGNAL_FUNC(spinEventCB), (gpointer)dna_data) ;
  g_signal_connect(GTK_OBJECT(end_spinbox), "event",
   GTK_SIGNAL_FUNC(spinEventCB), (gpointer)dna_data) ;
  g_signal_connect(GTK_OBJECT(flanking_spinbox), "event",
   GTK_SIGNAL_FUNC(spinEventCB), (gpointer)dna_data) ;



  dna_data->start_spin = GTK_SPIN_BUTTON(start_spinbox) ;
  dna_data->end_spin = GTK_SPIN_BUTTON(end_spinbox) ;
  dna_data->flanking_spin = GTK_SPIN_BUTTON(flanking_spinbox) ;


  return frame ;
}



static gboolean checkCoords(DNASearchData dna_data)
{
  gboolean result = FALSE ;
  int start, end, block_start, block_end ;

  start = dna_data->dna_start - dna_data->dna_flanking ;
  end = dna_data->dna_end + dna_data->dna_flanking ;

  block_start = dna_data->block->block_to_sequence.block.x1;
  block_end   = dna_data->block->block_to_sequence.block.x2;

  zmapWindowCoordPairToDisplay(dna_data->window, dna_data->window->display_coordinates,
                               block_start, block_end, &block_start, &block_end) ;

  if ((start <= end)
      && ((start >= block_start) && (start <= block_end))
      && ((end >= block_start) && (end <= block_end)))
    result = TRUE ;

  return result ;
}


static void getDNA(DNASearchData dna_data)
{
  char *feature_txt = NULL ;
  char *err_text = NULL ;
  int start, end ;
  int block_start;

  if (checkCoords(dna_data))
    {
      block_start = dna_data->block->block_to_sequence.block.x1 ;

      if (dna_data->window->display_forward_coords)
        block_start = zmapWindowCoordToDisplay(dna_data->window, dna_data->window->display_coordinates, block_start) ;

      /* Convert to relative coords.... */
      start = dna_data->dna_start ;
      end = dna_data->dna_end ;
      zMapBlock2FeatureCoords(dna_data->block, &start, &end) ;
      start = start - dna_data->dna_flanking ;
      end   = end + dna_data->dna_flanking ;

      /* Note that gtk_entry returns "" for no text, _not_ NULL. */
      feature_txt = dna_data->entry_text ;
      feature_txt = g_strdup(feature_txt) ;
      feature_txt = g_strstrip(feature_txt) ;

      if (!feature_txt || !*feature_txt)
        {
          err_text = g_strdup("no feature name supplied but is required for FASTA file.") ;
        }
      else if (start < dna_data->block->block_to_sequence.block.x1
       || end > dna_data->block->block_to_sequence.block.x2)
        {
          err_text = g_strdup_printf("start/end +/- flanking must be within range %d -> %d",
                                     dna_data->block->block_to_sequence.block.x1,
                                     dna_data->block->block_to_sequence.block.x2) ;
        }

      if (err_text)
        {
          zMapMessage("Please correct and retry: %s", err_text) ;

          g_free(err_text) ;
        }
      else
        {
          if ((dna_data->dna = zMapFeatureGetDNA((ZMapFeatureAny)(dna_data->block), start, end, dna_data->revcomp)))
            {
              dna_data->sequence_start = start ;
              dna_data->sequence_end = end ;
            }
        }

      g_free(feature_txt) ;
    }

  return ;
}



/* This is not the way to do help, we should really used html and have a set of help files. */
static void helpCB(gpointer data, guint callback_action, GtkWidget *w)
{
  const char *title = "DNA Export Window" ;
  const char *help_text =
    "The ZMap DNA Export Window allows you to export a section of DNA from a block.\n"
    "The fields are filled in with the name and start/end coords of the feature you\n"
    "selected but you can type in your own values if you wish to change any of these fields.\n"
    "You can also specify a flanking sequence length which will be added to either end of the\n."
    "feature coordinates to extend the dna sequence to be exported." ;

  zMapGUIShowText(title, help_text, FALSE) ;

  return ;
}


/* Requests destroy of search window which will end up with gtk calling destroyCB(). */
static void requestDestroyCB(gpointer cb_data, guint callback_action, GtkWidget *widget)
{
  DNASearchData dna_data = (DNASearchData)cb_data ;

  gtk_widget_destroy(dna_data->toplevel) ;

  return ;
}


/* Called once the widget receives a destroy signal. */
static void destroyCB(GtkWidget *widget, gpointer cb_data)
{
  DNASearchData dna_data = (DNASearchData)cb_data ;

  g_ptr_array_remove(dna_data->window->dna_windows, (gpointer)dna_data->toplevel);

  return ;
}



/*
 * You should read this before you alter these routines.....
 *
 * Event and value handling is slightly complicated with the spinners.
 * There seems to be no recognised way of asking to be called once the
 * user stops spinning (releases the mouse pointer) so you have to fake
 * it by catching the button release yourself.
 *
 * Handling of when the user types in a value and presses the Return
 * key is also difficult, you can't tell from the normal spinner callback
 * that Return was pressed. If you use an event handler to catch the key press it's
 * called _before_ the spinner has updated its data so the event
 * handler does not see the value the user typed in. Hence the key
 * press handler has to signal to the spin handler that its been called
 * and then the spin handler can be made to call our update routine.
 *
 * Note also that the spin handlers are not called if the new value in the
 * spinner is the same as the old one, sounds crazy but this is _exactly_
 * what happens when the user enters an out of range value and the spinner
 * itself then resets the value. Do this twice in a row and the new value is the same
 * as the old one. Its confusing...sigh...
 *
 */
static void startSpinCB(GtkSpinButton *spin_button, gpointer user_data)
{
  DNASearchData dna_data = (DNASearchData)user_data ;

  dna_data->dna_start = gtk_spin_button_get_value_as_int(spin_button) ;

  if (dna_data->enter_pressed == TRUE)
    {
      dna_data->enter_pressed = FALSE ;

      updateSpinners(dna_data) ;
    }

  return ;
}

static void endSpinCB(GtkSpinButton *spin_button, gpointer user_data)
{
  DNASearchData dna_data = (DNASearchData)user_data ;

  dna_data->dna_end = gtk_spin_button_get_value_as_int(spin_button) ;

  if (dna_data->enter_pressed == TRUE)
    {
      dna_data->enter_pressed = FALSE ;

      updateSpinners(dna_data) ;
    }


  return ;
}


static void flankingSpinCB(GtkSpinButton *spin_button, gpointer user_data)
{
  DNASearchData dna_data = (DNASearchData)user_data ;

  dna_data->dna_flanking = gtk_spin_button_get_value_as_int(spin_button) ;

  if (dna_data->enter_pressed == TRUE)
    {
      dna_data->enter_pressed = FALSE ;

      updateSpinners(dna_data) ;
    }

  return ;
}

/* Detects either that the user was using the mouse to update the spinner values and has finished
 * (button release) or that they typed in a value and pressed the Return key. If they pressed
 * the return key we simply flag that this has happened because the spinner callback will
 * be called next and its not until this happens that the spinner will have been updated with the new
 * spinner value. */
static gboolean spinEventCB(GtkWidget *widget, GdkEventClient *event, gpointer user_data)
{
  gboolean event_handled = FALSE ;    /* FALSE means other handlers run. */
  DNASearchData dna_data = (DNASearchData)user_data ;

  switch (event->type)
    {
    case GDK_KEY_PRESS:
      {
        GdkEventKey *key_event = (GdkEventKey *)event ;

        if (key_event->keyval == GDK_Return)
          {
            dna_data->enter_pressed = TRUE ;
          }

        break ;
      }
    case GDK_BUTTON_RELEASE:
      {
        updateSpinners(dna_data) ;

        break;
      }
    default:
      {
        break ;
      }
    }

  return event_handled ;
}


/* This function is called every time the user _finishes_ updating a spinner, it is not
 * called while the spinner is being spun. This is deliberate because the code as well
 * as keeping spinner values consistent with each other, also draws the extent of the
 * sequence selected by the user on the zmap window, we want this only to happen when
 * the users has set their final values.
 *
 * We know all we need to know to keep all the spinners logically consistent with each other,
 * this is done in this routine and ensures that we don't end up coords outside the block or
 * any other pathological values. */
static void updateSpinners(DNASearchData dna_data)
{
  int start, end, start_flank, end_flank, flanking_max ;
  int max_end, min_start;

  /* Get current spin values. */
  start    = gtk_spin_button_get_value_as_int(dna_data->start_spin) ;
  end      = gtk_spin_button_get_value_as_int(dna_data->end_spin) ;

  min_start =  dna_data->block->block_to_sequence.block.x1;
  max_end   =  dna_data->block->block_to_sequence.block.x2;

  zmapWindowCoordPairToDisplay(dna_data->window, dna_data->window->display_coordinates,
                               min_start, max_end, &min_start, &max_end) ;

  /* Set the ranges for all spin buttons according to current values, this causes the spinners
   * to reset any wayward values entered by the user. */
  gtk_spin_button_set_range(dna_data->start_spin,
                            (double)min_start,
                            (double)end) ;

  gtk_spin_button_set_range(dna_data->end_spin,
                            (double)start,
                            (double)max_end) ;

  start_flank = (start - min_start + 1) ;
  end_flank   = (end   - min_start + 1) ;
  if (start_flank < end_flank)
    flanking_max = start_flank ;
  else
    flanking_max = end_flank ;
  gtk_spin_button_set_range(dna_data->flanking_spin,
                            0.0,
                            (double)(flanking_max - 1)) ;

  /* Now get the new values which should be completely logically consistent. */
  dna_data->dna_start    = gtk_spin_button_get_value_as_int(dna_data->start_spin) ;
  dna_data->dna_end      = gtk_spin_button_get_value_as_int(dna_data->end_spin) ;
  dna_data->dna_flanking = gtk_spin_button_get_value_as_int(dna_data->flanking_spin) ;

#if OVERLAY
  /* Show the extent of the dna to be exported on the zmap window. */
  zMapDrawBoxChangeSize(dna_data->overlay_box,
                        dna_data->item_x1,
                        dna_data->dna_start - dna_data->dna_flanking,
                        dna_data->item_x2,
                        dna_data->dna_end + dna_data->dna_flanking) ;
#else
    zmapWindowHighlightSequenceItem(dna_data->window,  dna_data->seq_item,
                                dna_data->block->block_to_sequence.block.x1 + dna_data->dna_start,
                                dna_data->block->block_to_sequence.block.x1 + dna_data->dna_end, dna_data->dna_flanking);
#endif
  return ;
}

