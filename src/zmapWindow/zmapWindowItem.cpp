/*  File: zmapWindowItem.c
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
 * Description: Functions to manipulate canvas items.
 *
 * Exported functions: See zmapWindow_P.h
 *-------------------------------------------------------------------
 */

#include <ZMap/zmap.hpp>

#include <math.h>
#include <ZMap/zmapUtils.hpp>
#include <ZMap/zmapGLibUtils.hpp>
#include <ZMap/zmapSequence.hpp>
#include <zmapWindow_P.hpp>
#include <zmapWindowContainerUtils.hpp>


/* Why are these here....try to get rid of them.... */
#include <zmapWindowCanvasItem.hpp>
#include <zmapWindowContainerFeatureSet_I.hpp>



static void getVisibleCanvas(ZMapWindow window,
                             double *screenx1_out, double *screeny1_out,
                             double *screenx2_out, double *screeny2_out) ;


static void newGetWorldBounds(FooCanvasItem *item,
                              double *rootx1_out, double *rooty1_out,
                              double *rootx2_out, double *rooty2_out) ;






/*
 *                        External functions.
 */



gboolean zmapWindowItemGetStrandFrame(FooCanvasItem *item, ZMapStrand *set_strand, ZMapFrame *set_frame)
{
  ZMapWindowContainerFeatureSet container;
  ZMapFeature feature ;
  gboolean result = FALSE ;

  /* Retrieve the feature item info from the canvas item. */
  feature = zmapWindowItemGetFeature(item);
  if (!feature || (feature->struct_type != ZMAPFEATURE_STRUCT_FEATURE))
    return result ;

  container = (ZMapWindowContainerFeatureSet)zmapWindowContainerCanvasItemGetContainer(item) ;

  *set_strand = container->strand ;
  *set_frame  = container->frame ;

  result = TRUE ;

  return result ;
}


/* function is called from 2 places only, may be good to make callers use the id2c list directly
   this is a quick mod to preserve the existing interface */

GList *zmapWindowItemListToFeatureList(GList *item_list)
{
  GList *feature_list = NULL;
  ID2Canvas id2c;

  for(;item_list;item_list = item_list->next)
  {
          id2c = (ID2Canvas) item_list->data;
          feature_list = g_list_append(feature_list,(gpointer) id2c->feature_any);
  }

  return feature_list;
}


/* as above but replaces composite features with the underlying */
GList *zmapWindowItemListToFeatureListExpanded(GList *item_list, int expand)
{
  GList *feature_list = NULL;
  ID2Canvas id2c;
  ZMapFeature feature;

  for(;item_list;item_list = item_list->next)
    {
      id2c = (ID2Canvas) item_list->data;
      feature = (ZMapFeature) id2c->feature_any;

      if(expand && feature->children)
        {
          GList * l;

          for(l = feature->children; l; l = l->next)
            {
              feature_list = g_list_prepend(feature_list,(gpointer) l->data);
            }
        }
      else
        {
          feature_list = g_list_prepend(feature_list,(gpointer) id2c->feature_any);
        }
    }

  return feature_list;
}






/*
 *                     Feature Item highlighting....
 */

/* Highlight the given feature. */
void zMapWindowHighlightFeature(ZMapWindow window, ZMapFeature feature,
                                gboolean highlight_same_names, gboolean replace_flag)
{
  FooCanvasItem *feature_item ;
  ZMapStrand set_strand = ZMAPSTRAND_NONE;
  ZMapFrame set_frame = ZMAPFRAME_NONE;

  if(zMapStyleIsStrandSpecific(*feature->style))
        set_strand = feature->strand;
  if(zMapStyleIsFrameSpecific(*feature->style) && IS_3FRAME_COLS(window->display_3_frame))
        set_frame = zmapWindowFeatureFrame(feature);

  if ((feature_item = zmapWindowFToIFindFeatureItem(window, window->context_to_item,
                                                    set_strand, set_frame, feature)))
    zmapWindowHighlightObject(window, feature_item, replace_flag, highlight_same_names, FALSE) ;

  return ;
}



/* Unhighlight the given feature. */
gboolean zMapWindowUnhighlightFeature(ZMapWindow window, ZMapFeature feature)
{
  gboolean result = FALSE ;
  FooCanvasItem *feature_item ;
  ZMapStrand set_strand = ZMAPSTRAND_NONE ;
  ZMapFrame set_frame = ZMAPFRAME_NONE ;

  if (zMapStyleIsStrandSpecific(*feature->style))
    set_strand = feature->strand;
  if (zMapStyleIsFrameSpecific(*feature->style) && IS_3FRAME_COLS(window->display_3_frame))
    set_frame = zmapWindowFeatureFrame(feature);

  if ((feature_item = zmapWindowFToIFindFeatureItem(window, window->context_to_item,
                                                    set_strand, set_frame, feature))
      && feature_item == zmapWindowFocusGetHotItem(window->focus))
    {
      zmapWindowFocusRemoveFocusItem(window->focus, feature_item) ;
      result = TRUE ;
    }

  return result ;
}




/* Highlight a feature or list of related features (e.g. all hits for same query sequence). */
void zMapWindowHighlightObject(ZMapWindow window, FooCanvasItem *item,
                               gboolean replace_highlight_item, gboolean highlight_same_names,
                               ZMapFeatureSubPart sub_part)
{
  zmapWindowHighlightObject(window, item, replace_highlight_item, highlight_same_names, sub_part) ;

  return ;
}


/* Finds the feature item in a window corresponding to the supplied feature item...which is
 * usually one from a different window....
 *
 * This routine can return NULL if the user has two different sequences displayed and hence
 * there will be items in one window that are not present in another.
 */
FooCanvasItem *zMapWindowFindFeatureItemByItem(ZMapWindow window, FooCanvasItem *item)
{
  FooCanvasItem *matching_item = NULL ;
  ZMapFeature feature ;
  ZMapWindowContainerFeatureSet container;

  /* Retrieve the feature item info from the canvas item. */
  if ((feature = zMapWindowCanvasItemGetFeature(item)))
    {
      container = (ZMapWindowContainerFeatureSet)zmapWindowContainerCanvasItemGetContainer(item) ;

      matching_item = zmapWindowFToIFindFeatureItem(window,window->context_to_item,
                                                    container->strand, container->frame,
                                                    feature) ;
    }

  return matching_item ;
}









//
//                Package routines.
//


void zmapWindowHighlightObject(ZMapWindow window, FooCanvasItem *item,
                               gboolean replace_highlight_item, gboolean highlight_same_names,
                               ZMapFeatureSubPart sub_part)
{
  ZMapWindowCanvasItem canvas_item ;
  ZMapFeature feature ;


  canvas_item = zMapWindowCanvasItemIntervalGetObject(item) ;
  if (!ZMAP_IS_CANVAS_ITEM(canvas_item))
    return ;


  /* Retrieve the feature item info from the canvas item. */
  feature = zmapWindowItemGetFeature((FooCanvasItem *)canvas_item);
  if (!feature)
    return ;


  /* If any other feature(s) is currently in focus, revert it to its std colours */
  if (replace_highlight_item)
    zmapWindowUnHighlightFocusItems(window) ;


  /* For some types of feature we want to highlight all the ones with the same name in that column. */
  switch (feature->mode)
    {
    case ZMAPSTYLE_MODE_ALIGNMENT:
      {
        if (highlight_same_names)
          {
            GList *set_items ;
            const char *set_strand, *set_frame ;

            set_strand = set_frame = "*" ;

            set_items = zmapWindowFToIFindSameNameItems(window,window->context_to_item,
                                                        set_strand, set_frame, feature) ;

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
            if (set_items)
              zmapWindowFToIPrintList(set_items) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */

            zmapWindowFocusAddItems(window->focus, set_items, item, feature) ; // item is the hot one

            g_list_free(set_items) ;
          }
        else
          {
            zmapWindowFocusAddItem(window->focus, item , feature) ;
          }

        break ;
      }
    default:
      {
        zmapWindowFocusAddItemType(window->focus, item, feature, sub_part, WINDOW_FOCUS_GROUP_FOCUS) ;
      }
      break ;
    }



  /* Highlight DNA and Peptide sequences corresponding to feature (if visible),
   * note we only do this if the feature is forward or non-stranded, makes no sense otherwise. */
  if (zMapStyleIsStrandSpecific(*feature->style) && ZMAPFEATURE_REVERSE(feature))
    {
      /* Deselect any already selected sequence as focus item is now the reverse strand item. */
      zmapWindowItemUnHighlightDNA(window, item) ;

      zmapWindowItemUnHighlightTranslations(window, item) ;

      zmapWindowItemUnHighlightShowTranslations(window, item) ;
    }
  else
    {
      ZMapFrame frame = ZMAPFRAME_NONE ;
      gboolean sub_part_tmp = FALSE ;

      if (sub_part)
        sub_part_tmp = TRUE ;

      zmapWindowItemHighlightDNARegion(window, TRUE, sub_part_tmp, item,
                                       ZMAPFRAME_NONE, ZMAPSEQUENCE_NONE, feature->x1, feature->x2, 0) ;

      zmapWindowItemHighlightTranslationRegions(window, TRUE, sub_part_tmp, item,
                                                ZMAPFRAME_NONE, ZMAPSEQUENCE_NONE, feature->x1, feature->x2, 0) ;


      /* PROBABLY NEED TO DISABLE THIS UNTIL I CAN GET IT ALL WORKING...... */

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
      /* Not completely happy with this...seems a bit hacky, try it and if users want to do
       * translations non-transcripts then I'll change it. */
      if (ZMAPFEATURE_IS_TRANSCRIPT(feature))
        zmapWindowItemHighlightShowTranslationRegion(window, TRUE, sub_part, item,
                                                     ZMAPFRAME_NONE, ZMAPSEQUENCE_NONE, feature->x1, feature->x2) ;
      else
        zmapWindowItemUnHighlightShowTranslations(window, item) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


      zmapWindowItemHighlightShowTranslationRegion(window, TRUE, sub_part_tmp, item,
                                                   frame, ZMAPSEQUENCE_NONE, feature->x1, feature->x2, 0) ;



    }




  zmapWindowHighlightFocusItems(window);

  return ;
}




void zmapWindowHighlightFocusItems(ZMapWindow window)
{
  FooCanvasItem *hot_item ;
  FooCanvasGroup *hot_column = NULL;

  /* If any other feature(s) is currently in focus, revert it to its std colours */
  if ((hot_column = zmapWindowFocusGetHotColumn(window->focus)))
    zmapWindowFocusHighlightHotColumn(window->focus) ;

  if ((hot_item = zmapWindowFocusGetHotItem(window->focus)))
    zmapWindowFocusHighlightFocusItems(window->focus, window) ;


  return ;
}



void zmapWindowUnHighlightFocusItems(ZMapWindow window)
{
//  FooCanvasItem *hot_item ;
//  FooCanvasGroup *hot_column ;

  /* If any other feature(s) is currently in focus, revert it to its std colours */
//  hot_column = zmapWindowFocusGetHotColumn(window->focus);
// if (hot_column)
//  zmapWindowFocusUnHighlightHotColumn(window) ;     // done by reset

/* this sets the feature, loosing the currently selected one */
//if ((hot_item = zmapWindowFocusGetHotItem(window->focus)))
//    zmapWindowFocusUnhighlightFocusItems(window->focus, window) ;

//  if (hot_column || hot_item)

/* don't need any of the above! */
    zmapWindowFocusReset(window->focus) ;

  return ;
}


/*
 *                         Functions to get hold of items in various ways.
 */


ZMapFeatureBlock zmapWindowItemGetFeatureBlock(FooCanvasItem *item)
{
  ZMapFeatureBlock block = NULL ;

  block = (ZMapFeatureBlock)zmapWindowItemGetFeatureAnyType(item, ZMAPFEATURE_STRUCT_BLOCK) ;

  return block ;
}


ZMapFeature zmapWindowItemGetFeature(FooCanvasItem *item)
{
  ZMapFeature feature = NULL ;

  feature = (ZMapFeature)zmapWindowItemGetFeatureAnyType(item, ZMAPFEATURE_STRUCT_FEATURE) ;

  return feature ;
}


ZMapFeatureAny zmapWindowItemGetFeatureAny(FooCanvasItem *item)
{
  ZMapFeatureAny feature_any = NULL ;

  /* -1 means don't check */
  feature_any = zmapWindowItemGetFeatureAnyType(item, -1) ;

  return feature_any ;
}


ZMapFeatureAny zmapWindowItemGetFeatureAnyType(FooCanvasItem *item, const int expected_type)
{
  ZMapFeatureAny feature_any = NULL ;
  ZMapWindowCanvasItem feature_item ;



  /* THE FIRST AND SECOND if's SHOULD BE MOVED TO THE items subdirectory which should have a
   * function that returns the feature given an item or a subitem... */
  if (item && ZMAP_IS_CONTAINER_GROUP(item))
    {
      zmapWindowContainerGetFeatureAny((ZMapWindowContainerGroup)item, &feature_any) ;
    }
  else if (item && (feature_item = zMapWindowCanvasItemIntervalGetObject(item)))
    {
      feature_any = (ZMapFeatureAny)zMapWindowCanvasItemGetFeature(FOO_CANVAS_ITEM(feature_item)) ;
    }
  else
    {
      zMapLogMessage("Unexpected item [%s]", (item ? G_OBJECT_TYPE_NAME(item) : "null")) ;
    }

  if (feature_any && expected_type != -1)
    {
      if (feature_any->struct_type != expected_type)
        {
          zMapLogCritical("Unexpected feature type [%d] (not %d) attached to item [%s]",
                          feature_any->struct_type, expected_type, G_OBJECT_TYPE_NAME(item)) ;
          feature_any = NULL;
        }
    }

  return feature_any ;
}


/*! \todo #warning this function does nothing but is called five times and should be removed */
/* Get "parent" item of feature, for simple features, this is just the item itself but
 * for compound features we need the parent group.
 *  */
FooCanvasItem *zmapWindowItemGetTrueItem(FooCanvasItem *item)
{
  FooCanvasItem *true_item = NULL ;

  true_item = item;

  return true_item ;
}






/*
 * MANY OF THESE FUNCTIONS ALL FEEL LIKE THEY SHOULD BE IN THE items directory somewhere.....
 * THERE IS MUCH CODE THAT SHOULD BE MOVED FROM HERE INTO THERE.....
 */


/* Returns the container parent of the given canvas item which may not be feature, it might be
   some decorative box, e.g. as in the alignment colinear bars. */
FooCanvasGroup *zmapWindowItemGetParentContainer(FooCanvasItem *feature_item)
{
  FooCanvasGroup *parent_container = NULL ;

  parent_container = (FooCanvasGroup *)zmapWindowContainerCanvasItemGetContainer(feature_item);

  return parent_container ;
}




#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
/* Finds the feature item child in a window corresponding to the supplied feature item..which is
 * usually one from a different window....
 * A feature item child is something like the feature item representing an exon within a transcript. */
FooCanvasItem *zMapWindowFindFeatureItemChildByItem(ZMapWindow window, FooCanvasItem *item,
                                                    int child_start, int child_end)
{
  FooCanvasItem *matching_item = NULL ;
  ZMapFeature feature ;
  ZMapWindowContainerFeatureSet container;

  zMapReturnValIfFail((!window || !item || (child_start <= 0) || (child_end <= 0) || (child_start > child_end)),
                      matching_item) ;

  /* Retrieve the feature item info from the canvas item. */
  if ((feature = zmapWindowItemGetFeature(item)))
    {
      container = (ZMapWindowContainerFeatureSet)zmapWindowContainerCanvasItemGetContainer(item) ;

      /* Find the item that matches */
      matching_item = zmapWindowFToIFindFeatureItem(window,window->context_to_item,
                                                    container->strand, container->frame, feature) ;
    }

  return matching_item ;
}
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



/*
 *                  Testing items visibility and scrolling to those items.
 */

/* Checks to see if the item really is visible.  In order to do this
 * all the item's parent groups need to be examined.
 *
 *  mh17:not called from anywhere
 */
gboolean zmapWindowItemIsVisible(FooCanvasItem *item)
{
  gboolean visible = FALSE;

  visible = zmapWindowItemIsShown(item);

#ifdef RDS_DONT
  /* we need to check out our parents :( */
  /* we would like not to do this */
  if(visible)
    {
      FooCanvasGroup *parent = NULL;
      parent = FOO_CANVAS_GROUP(item->parent);
      while(visible && parent)
        {
          visible = zmapWindowItemIsShown(FOO_CANVAS_ITEM(parent));
          /* how many parents we got? */
          parent  = FOO_CANVAS_GROUP(FOO_CANVAS_ITEM(parent)->parent);
        }
    }
#endif

  return visible;
}


/* Checks to see if the item is shown.  An item may still not be
 * visible as any one of its parents might be hidden. If a
 * definitive answer is required use zmapWindowItemIsVisible instead. */
gboolean zmapWindowItemIsShown(FooCanvasItem *item)
{
  gboolean visible = FALSE ;

  if (FOO_IS_CANVAS_ITEM(item))
    {
      g_object_get(G_OBJECT(item),
                   "visible", &visible,
                   NULL) ;
    }

  return visible ;
}



/* Tests to see whether an item is visible on the screen. If "completely" is TRUE then the
 * item must be completely on the screen otherwise FALSE is returned. */
gboolean zmapWindowItemIsOnScreen(ZMapWindow window, FooCanvasItem *item, gboolean completely)
{
  gboolean is_on_screen = FALSE ;
  double screenx1_out, screeny1_out, screenx2_out, screeny2_out ;
  double itemx1_out, itemy1_out, itemx2_out, itemy2_out ;

  /* Work out which part of the canvas is visible currently. */
  getVisibleCanvas(window, &screenx1_out, &screeny1_out, &screenx2_out, &screeny2_out) ;

  /* Get the items world coords. */
  my_foo_canvas_item_get_world_bounds(item, &itemx1_out, &itemy1_out, &itemx2_out, &itemy2_out) ;



  /* Compare them. */
  if (completely)
    {
      if (itemx1_out >= screenx1_out && itemx2_out <= screenx2_out
          && itemy1_out >= screeny1_out && itemy2_out <= screeny2_out)
        is_on_screen = TRUE ;
      else
        is_on_screen = FALSE ;
    }
  else
    {
      if (itemx2_out < screenx1_out || itemx1_out > screenx2_out
          || itemy2_out < screeny1_out || itemy1_out > screeny2_out)
        is_on_screen = FALSE ;
      else
        is_on_screen = TRUE ;
    }

  return is_on_screen ;
}




/* Moves to the given item and optionally changes the size of the scrolled region
 * in order to display the item. */
void zmapWindowItemCentreOnItem(ZMapWindow window, FooCanvasItem *item,
                                gboolean alterScrollRegionSize,
                                double boundaryAroundItem)
{
  zmapWindowItemCentreOnItemSubPart(window, item, alterScrollRegionSize, boundaryAroundItem, 0.0, 0.0) ;

  return ;
}



// THIS FUNCTION SEEMS UTTERLY, UTTERLY BROKEN..............

/* Moves to a subpart of an item, note the coords sub_start/sub_end need to be item coords,
 * NOT world coords. If sub_start == sub_end == 0.0 then the whole item is centred on. */
/* NOTE (mh17) must be sequence coordinates
 * (not world)
 */

void zmapWindowItemCentreOnItemSubPart(ZMapWindow window, FooCanvasItem *item,
                                       gboolean alterScrollRegionSize,
                                       double boundaryAroundItem,
                                       double sub_start, double sub_end)
{
  double ix1, ix2, iy1, iy2;
  int cx1, cx2, cy1, cy2;
  int final_canvasx, final_canvasy, tmpx, tmpy, cheight, cwidth;
  static gboolean debug = FALSE ;


  /* OH GOSH....THIS FUNCTION IS HARD TO FOLLOW, SOME COMMENTING WOULD HAVE HELPED. */
  if (!window || !item)
    return ;

  if (zmapWindowItemIsShown(item))
    {
      /* THIS CODE IS NOT GREAT AND POINTS TO SOME FUNDAMENTAL ROUTINES/UNDERSTANDING THAT IS
       * MISSING.....WE NEED TO SORT OUT WHAT SIZE WE NEED TO CALCULATE FROM AS OPPOSED TO
       * WHAT THE CURRENTLY DISPLAYED OBJECT SIZE IS, THIS MAY HAVE BEEN TRUNCATED BY THE
       * THE LONG ITEM CLIP CODE OR JUST BY CHANGING THE SCROLLED REGION. */

      foo_canvas_item_get_bounds(item, &ix1, &iy1, &ix2, &iy2) ;

#if NOT_RELEVANT
/* this will never be a group now */
      /* If the item is a group then we need to use its background to check in long items as the
       * group itself is not a long item. */
      if (FOO_IS_CANVAS_GROUP(item) && zmapWindowContainerUtilsIsValid(FOO_CANVAS_GROUP(item)))
        {
          double height ;
          foo_canvas_item_get_bounds(item, &ix1, &iy1, &ix2, &iy2) ;

          height = iy2 - iy1 + 1;
          if (iy1 > 0)

            iy1 = 0 ;
          if (iy2 < height)
            iy2 = height ;
        }
      else
        {
          foo_canvas_item_get_bounds(item, &ix1, &iy1, &ix2, &iy2) ;
        }
#endif


      /* If sub_part start/end are set then need to centre on that part of an item. */
      if (sub_start != 0.0 || sub_end != 0.0)
        {
#if USING_ITEM_COORDS
          iy2 = iy1 ;
          iy1 = iy1 + sub_start ;
          iy2 = iy2 + sub_end ;
#else
          iy1 = sub_start;
          iy2 = sub_end;
#endif
        }

#if USING_ITEM_COORDS
        /* Fix the item coords to be in world coords for following calculations. */
      foo_canvas_item_i2w(item, &ix1, &iy1);
      foo_canvas_item_i2w(item, &ix2, &iy2);
#endif


      if (boundaryAroundItem > 0.0)
        {
          /* sadly I don't know what PPU stand for.... */

          /* think about PPU for X & Y!! */
          ix1 -= boundaryAroundItem;
          iy1 -= boundaryAroundItem;
          ix2 += boundaryAroundItem;
          iy2 += boundaryAroundItem;
        }


      if (alterScrollRegionSize)
        {
          /* this doeesn't work. don't know why. */
          double sy1, sy2;
          sy1 = iy1; sy2 = iy2;
          zmapWindowClampSpan(window, &sy1, &sy2);
          zMapWindowMove(window, sy1, sy2);
        }
      else
        {
        double sx1, sx2, sy1, sy2, tmps, tmpi, diff;
              /* Get scroll region (clamped to sequence coords) */
          zmapWindowGetScrollableArea(window, &sx1, &sy1, &sx2, &sy2);

//          if (!zmapWindowItemRegionIsVisible(window, item))
          if(iy1 < sy1 || iy2 > sy2)        /* as iy1 < iy2 that's all we need to test */
            {

              /* we now have scroll region coords */
              /* set tmp to centre of regions ... */
              tmps = sy1 + ((sy2 - sy1) / 2);
              tmpi = iy1 + ((iy2 - iy1) / 2);

              /* find difference between centre points */
              diff = tmps - tmpi;

              /* alter scroll region to match that */
              sy1 -= diff; sy2 -= diff;

              /* clamp in case we've moved outside the sequence */
              zmapWindowClampSpan(window, &sy1, &sy2);

              /* Do the move ... */
              /*! \todo #warning this assumes the scroll region is big enough for the DNA match, but some people have searched for big sequences */
              zMapWindowMove(window, sy1, sy2);
            }
        }



      /* THERE IS A PROBLEM WITH HORIZONTAL SCROLLING HERE, THE CODE DOES NOT SCROLL FAR
       * ENOUGH TO THE RIGHT...I'M NOT SURE WHY THIS IS AT THE MOMENT. */


      /* get canvas coords to work with */
      foo_canvas_w2c(item->canvas, ix1, iy1, &cx1, &cy1);
      foo_canvas_w2c(item->canvas, ix2, iy2, &cx2, &cy2);

      if (debug)
        {
          printf("[zmapWindowItemCentreOnItem] ix1=%f, ix2=%f, iy1=%f, iy2=%f\n", ix1, ix2, iy1, iy2);
          printf("[zmapWindowItemCentreOnItem] cx1=%d, cx2=%d, cy1=%d, cy2=%d\n", cx1, cx2, cy1, cy2);
        }

      /* This should possibly be a function...YEH, WHAT DOES IT DO.....????? */
      tmpx = cx2 - cx1;
      tmpy = cy2 - cy1;

      if(tmpx & 1)
        tmpx += 1;
      if(tmpy & 1)
        tmpy += 1;

      final_canvasx = cx1 + (tmpx / 2);
      final_canvasy = cy1 + (tmpy / 2);

      tmpx = GTK_WIDGET(window->canvas)->allocation.width;
      tmpy = GTK_WIDGET(window->canvas)->allocation.height;

      if(tmpx & 1)
        tmpx -= 1;
      if(tmpy & 1)
        tmpy -= 1;

      cwidth = tmpx / 2;
      cheight = tmpy / 2;
      final_canvasx -= cwidth;
      final_canvasy -= cheight;

      if(debug)
        {
          printf("[zmapWindowItemCentreOnItem] cwidth=%d  cheight=%d\n", cwidth, cheight);
          printf("[zmapWindowItemCentreOnItem] scrolling to"
                 " canvas x=%d y=%d\n",
                 final_canvasx, final_canvasy);
        }

      /* Scroll to the item... */
      foo_canvas_scroll_to(window->canvas, final_canvasx, final_canvasy);
    }

  return ;
}


// PLEASE NOTE.....SINCE MALCOLM'S CHANGES THIS FUNCTION IS ONLY NEEDED TO SCROLL FROM COLUMN TO
// COLUMN AS INDIVIDUAL FEATURES ARE NO LONGER CANVAS FEATURES. THEREFORE THE SCROLLING IN THE
// Y-AXIS HAS BEEN DISABLED....AND THERE IS IN FACT A GOOD REASON FOR THIS, IT WOULD APPEAR THAT
// SINCE THE RETIREMENT OF FEATURES AS FULL CANVAS FEATURES THE COORDINATE SYSTEM FOR THE COLUMN
// GROUPS HAS BECOME COMPLETELY MESSED UP SO THAT THE SIZE OF THE BACKGROUND COLUMN FEATURE NO
// LONGER BEARS ANY RESEMBLANCE TO THE COORDINATES OF THE FEATURES !!
//
/* Scrolls to an item if that item is not visible on the scren.
 *
 * NOTE: scrolling is only done in the axis in which the item is completely
 * invisible, the other axis is left unscrolled so that the visible portion
 * of the feature remains unaltered.
 *  */
void zmapWindowScrollToItem(ZMapWindow window, FooCanvasItem *item)
{
  double screenx1_out, screeny1_out, screenx2_out, screeny2_out ;
  double itemx1_out, itemy1_out, itemx2_out, itemy2_out ;
  gboolean do_x = FALSE, do_y = FALSE ;
  double x_offset = 0.0, y_offset = 0.0;
  int curr_x, curr_y ;

  /* Work out which part of the canvas is visible currently. */
  getVisibleCanvas(window, &screenx1_out, &screeny1_out, &screenx2_out, &screeny2_out) ;


  x_offset = screenx1_out ;
  y_offset = screeny1_out ;


  /* Get the items world coords. */
  newGetWorldBounds(item, &itemx1_out, &itemy1_out, &itemx2_out, &itemy2_out) ;



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  // debug....
  double x1 = itemx1_out, y1 = itemy1_out, x2 = itemx2_out, y2 = itemy2_out ;


  foo_canvas_w2c_rect_d(window->canvas,
                        &x1, &y1,
                        &x2, &y2);

  /* Get the current scroll offsets in world coords. */
  foo_canvas_get_scroll_offsets(window->canvas, &curr_x, &curr_y) ;
  foo_canvas_c2w(window->canvas, curr_x, curr_y, &x_offset, &y_offset) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */



  /* Work out whether any scrolling is required. */
  if (itemx1_out > screenx2_out || itemx2_out < screenx1_out)
    {
      do_x = TRUE ;

      if (itemx1_out > screenx2_out)
        {
          x_offset = itemx1_out ;
        }
      else if (itemx2_out < screenx1_out)
        {
          x_offset = itemx1_out ;
        }
    }


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  if (itemy1_out > screeny2_out || itemy2_out < screeny1_out)
    {
      do_y = TRUE ;

      if (itemy1_out > screeny2_out)
        {
          y_offset = itemy1_out ;
        }
      else if (itemy2_out < screeny1_out)
        {
          y_offset = itemy1_out ;
        }
    }
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  /* If we need to scroll then do it. */
  if (do_x || do_y)
    {
      foo_canvas_w2c(window->canvas, x_offset, y_offset, &curr_x, &curr_y) ;

      foo_canvas_scroll_to(window->canvas, curr_x, curr_y) ;
    }

  return ;
}



/* Scrolls to a feature if that feature is not visible on the screen.
 *
 * NOTE: scrolling is only done in the axis in which the item is completely
 * invisible, the other axis is left unscrolled so that the visible portion
 * of the feature remains unaltered.
 *  */
void zmapWindowScrollToFeature(ZMapWindow window,
                               FooCanvasItem *feature_column_item, ZMapWindowCanvasFeature canvas_feature)
{

  ZMapFeature feature ;
  double feature_offset, feature_width ;
  double screenx1_out, screeny1_out, screenx2_out, screeny2_out ;
  double itemx1_out, itemy1_out, itemx2_out, itemy2_out ;
  double feature_start_w = 0.0, feature_end_w = 0.0 ;
  gboolean do_x = FALSE, do_y = FALSE ;
  double x_position, y_position ;
  int curr_x, curr_y ;

  
  feature = zMapWindowCanvasFeatureGetFeature(canvas_feature) ;
  feature_offset = zMapWindowCanvasFeatureGetBumpOffset(canvas_feature) ;
  feature_width = zMapWindowCanvasFeatureGetWidth(canvas_feature) ;

  /* Work out which part of the canvas is visible currently in world bounds. */
  getVisibleCanvas(window, &screenx1_out, &screeny1_out, &screenx2_out, &screeny2_out) ;

  /* Get the items world coords. */
  newGetWorldBounds(feature_column_item, &itemx1_out, &itemy1_out, &itemx2_out, &itemy2_out) ;

  // Get the features start/end in world coords
  zmapWindowSeqToWorldCoords(window,
                             feature->x1, feature->x2, &feature_start_w, &feature_end_w) ;

  // Set y to features coords.
  itemy1_out = feature_start_w ;
  itemy2_out = feature_end_w ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  // Set x to features bump offset.
  double tmp_x1 = 0.0, tmp_x2 = 0.0 ;

  foo_canvas_w2c(window->canvas, x_position, y_position, &curr_x, NULL) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */




  itemx1_out += feature_offset ;
  itemx2_out = (itemx1_out + feature_width) ;


  x_position = itemx1_out ;
  y_position = itemy1_out ;






#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  // debug....
  double x1 = itemx1_out, y1 = itemy1_out, x2 = itemx2_out, y2 = itemy2_out ;


  foo_canvas_w2c_rect_d(window->canvas,
                        &x1, &y1,
                        &x2, &y2);

  /* Get the current scroll offsets in world coords. */
  foo_canvas_get_scroll_offsets(window->canvas, &curr_x, &curr_y) ;
  foo_canvas_c2w(window->canvas, curr_x, curr_y, &x_position, &y_position) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


  /* Work out whether any scrolling is required. */
  if (itemx1_out > screenx2_out || itemx2_out < screenx1_out)
    {
      do_x = TRUE ;

      if (itemx1_out > screenx2_out)
        {
          x_position = itemx1_out ;
        }
      else if (itemx2_out < screenx1_out)
        {
          x_position = itemx1_out ;
        }

      // Correct x to be half way up screen.....
      x_position -= ((screenx2_out - screenx1_out) / 2) ;

    }

  if (itemy1_out > screeny2_out || itemy2_out < screeny1_out)
    {
      do_y = TRUE ;

      if (itemy1_out > screeny2_out)
        {
          y_position = itemy1_out ;
        }
      else if (itemy2_out < screeny1_out)
        {
          y_position = itemy1_out ;
        }

      // Correct y to be half way up screen.....
      y_position -= ((screeny2_out - screeny1_out) / 2) ;

    }

  /* If we need to scroll then do it. */
  if (do_x || do_y)
    {
      foo_canvas_w2c(window->canvas, x_position, y_position, &curr_x, &curr_y) ;

      foo_canvas_scroll_to(window->canvas, curr_x, curr_y) ;

      // Correct x to scroll to 
    }

  return ;
}







/* Returns the sequence coords that correspond to the given _world_ foocanvas coords.
 *
 * NOTE that although we only return y coords, we need the world x coords as input
 * in order to find the right foocanvas item from which to get the sequence coords.
 *
 * Not so easy, we are poking around in the dark....
 *  */
gboolean zmapWindowWorld2SeqCoords(ZMapWindow window, FooCanvasItem *foo,
                                   double wx1, double wy1, double wx2, double wy2,
                                   FooCanvasGroup **block_grp_out, int *y1_out, int *y2_out)
{
  gboolean result = FALSE ;
  FooCanvasItem *item ;

  item = foo;
  if (!item)
    return result ;
#if 0
  if(!item)                /* should never be called: legacy code */
          item = foo_canvas_get_item_at(window->canvas, mid_x, mid_y);
#endif
  if(item)
    {
      FooCanvasGroup *block_container ;
      ZMapFeatureBlock block ;

      /* Getting the block struct as well is a bit belt and braces...we could return it but
       * its redundant info. really. */
      if ((block_container = FOO_CANVAS_GROUP(zmapWindowContainerUtilsItemGetParentLevel(item, ZMAPCONTAINER_LEVEL_BLOCK)))
          && (block = zmapWindowItemGetFeatureBlock((FooCanvasItem *)block_container)))
        {
          double offset ;

          offset = (double)(block->block_to_sequence.block.x1 - 1) ; /* - 1 for 1 based coord system. */

          my_foo_canvas_world_bounds_to_item(FOO_CANVAS_ITEM(block_container), &wx1, &wy1, &wx2, &wy2) ;

          if (block_grp_out)
            *block_grp_out = block_container ;

          if (y1_out)
            *y1_out = floor(wy1 + offset + 0.5) ;
          if (y2_out)
            *y2_out = floor(wy2 + offset + 0.5) ;

          result = TRUE ;
        }
      else
        zMapLogWarning("%s", "No Block Container");
    }

  return result ;
}


gboolean zmapWindowItem2SeqCoords(FooCanvasItem *item, int *y1, int *y2)
{
  gboolean result = FALSE ;



  return result ;
}



/* Convert sequence coords into world coords, can fail if sequence coords are out
 * of range. */
gboolean zmapWindowSeqToWorldCoords(ZMapWindow window,
                                    int seq_start, int seq_end, double *world_start_out, double *world_end_out)
{
  gboolean result = FALSE ;
  FooCanvasItem *set_item ;


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  ZMapFeatureAlignment align = NULL ;
  ZMapFeatureBlock block = NULL ;
  ZMapFeatureSet feature_set = NULL ;
  GQuark set_id ;

  /* We don't want caller to have to do all this so we laboriously find a featureset,
   * the first one in fact, and use that to call the canvas function to do the conversion
   * as the canvas knows how to do that. */
  align = zMap_g_hash_table_nth(window->feature_context->alignments, 0) ;
  block = zMap_g_hash_table_nth(align->blocks, 0) ;
  feature_set = zMap_g_hash_table_nth(block->feature_sets, 0) ;



#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  set_id = zMapStyleCreateID(ZMAP_FIXED_STYLE_STRAND_SEPARATOR) ;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
  set_id = zMapStyleCreateID(ZMAP_FIXED_STYLE_SEARCH_MARKERS_NAME) ;



  if ((set_item = zmapWindowFToIFindItemFull(window, window->context_to_item,
                                             align->unique_id, block->unique_id,

#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
                                             feature_set->unique_id,
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
                                             set_id,
                                             ZMAPSTRAND_FORWARD, ZMAPFRAME_NONE, 0)))
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */


    set_item = (FooCanvasItem *)(window->separator_feature_set) ;

    {
      ZMapWindowFeaturesetItem featureset ;
      double world_start, world_end ;

      featureset = (ZMapWindowFeaturesetItem)set_item ;

      if (zMapCanvasFeaturesetSeq2World(featureset,
                                        seq_start, seq_end, &world_start, &world_end))
        {
          *world_start_out = world_start ;
          *world_end_out = world_end ;

          result = TRUE ;
        }
    }

  return result ;
}





/**
 * my_foo_canvas_item_get_world_bounds:
 * @item: A canvas item.
 * @rootx1_out: X left coord of item (output value).
 * @rooty1_out: Y top coord of item (output value).
 * @rootx2_out: X right coord of item (output value).
 * @rootx2_out: Y bottom coord of item (output value).
 *
 * Returns the _world_ bounds of an item, no function exists in FooCanvas to do this.
 **/
void my_foo_canvas_item_get_world_bounds(FooCanvasItem *item,
                                         double *rootx1_out, double *rooty1_out,
                                         double *rootx2_out, double *rooty2_out)
{
  double rootx1, rootx2, rooty1, rooty2 ;

  g_return_if_fail (FOO_IS_CANVAS_ITEM (item)) ;
  g_return_if_fail (rootx1_out != NULL || rooty1_out != NULL || rootx2_out != NULL || rooty2_out != NULL ) ;

  foo_canvas_item_get_bounds(item, &rootx1, &rooty1, &rootx2, &rooty2) ;

  // debugging
  double new_rootx1 = rootx1, new_rootx2 = rootx2, new_rooty1 = rooty1, new_rooty2 = rooty2 ;

  foo_canvas_item_i2w(item->parent, &new_rootx1, &new_rooty1) ;
  foo_canvas_item_i2w(item->parent, &new_rootx2, &new_rooty2) ;



  foo_canvas_item_i2w(item, &rootx1, &rooty1) ;
  foo_canvas_item_i2w(item, &rootx2, &rooty2) ;



  if (rootx1_out)
    *rootx1_out = rootx1 ;
  if (rooty1_out)
    *rooty1_out = rooty1 ;
  if (rootx2_out)
    *rootx2_out = rootx2 ;
  if (rooty2_out)
    *rooty2_out = rooty2 ;

  return ;
}


// The above function does not return the results we want for our canvas...somehow we have weird
// coords within our group whereas the actual group position is correct......this seems to happen
// after any bumping which is probably a clue....
//
// This function tries to get round this by using the group position as the origin of
// its position calculation.
//
// THIS ISN'T SATISFACTORY and should be redone once the reason for the discrepancy is
// discovered.
//
// Returns the _world_ bounds of an item, no function exists in FooCanvas to do this.
//
static void newGetWorldBounds(FooCanvasItem *item,
                              double *rootx1_out, double *rooty1_out,
                              double *rootx2_out, double *rooty2_out)
{
  if (FOO_IS_CANVAS_GROUP(item))
    {
      FooCanvasGroup *group = (FooCanvasGroup *)item ;
      double x1, x2, y1, y2 ;
      double rootx1, rootx2, rooty1, rooty2 ;

      // Base our calculation on the group 
      x1 = group->xpos ;
      y1 = group->ypos ;

      foo_canvas_item_get_bounds(item, &rootx1, &rooty1, &rootx2, &rooty2) ;

      x2 = x1 + (rootx2 - rootx1) ;

      y2 = y1 + (rooty2 - rooty1) ;

      foo_canvas_item_i2w(item->parent, &x1, &y1) ;
      foo_canvas_item_i2w(item->parent, &x2, &y2) ;

      if (rootx1_out)
        *rootx1_out = x1 ;
      if (rooty1_out)
        *rooty1_out = y1 ;
      if (rootx2_out)
        *rootx2_out = x2 ;
      if (rooty2_out)
        *rooty2_out = y2 ;
    }
  else
    {
      printf("oh dear\n") ;


    }

  return ;
}



/**
 * my_foo_canvas_world_bounds_to_item:
 * @item: A canvas item.
 * @rootx1_out: X left coord of item (input/output value).
 * @rooty1_out: Y top coord of item (input/output value).
 * @rootx2_out: X right coord of item (input/output value).
 * @rootx2_out: Y bottom coord of item (input/output value).
 *
 * Returns the given _world_ bounds in the given item coord system, no function exists in FooCanvas to do this.
 **/
void my_foo_canvas_world_bounds_to_item(FooCanvasItem *item,
                                        double *rootx1_inout, double *rooty1_inout,
                                        double *rootx2_inout, double *rooty2_inout)
{
  double rootx1, rootx2, rooty1, rooty2 ;

  g_return_if_fail (FOO_IS_CANVAS_ITEM (item)) ;
  g_return_if_fail (rootx1_inout != NULL || rooty1_inout != NULL || rootx2_inout != NULL || rooty2_inout != NULL ) ;

  rootx1 = rootx2 = rooty1 = rooty2 = 0.0 ;

  if (rootx1_inout)
    rootx1 = *rootx1_inout ;
  if (rooty1_inout)
    rooty1 = *rooty1_inout ;
  if (rootx2_inout)
    rootx2 = *rootx2_inout ;
  if (rooty2_inout)
    rooty2 = *rooty2_inout ;

  foo_canvas_item_w2i(item, &rootx1, &rooty1) ;
  foo_canvas_item_w2i(item, &rootx2, &rooty2) ;

  if (rootx1_inout)
    *rootx1_inout = rootx1 ;
  if (rooty1_inout)
    *rooty1_inout = rooty1 ;
  if (rootx2_inout)
    *rootx2_inout = rootx2 ;
  if (rooty2_inout)
    *rooty2_inout = rooty2 ;

  return ;
}





/* I'M TRYING THESE TWO FUNCTIONS BECAUSE I DON'T LIKE THE BIT WHERE IT GOES TO THE ITEMS
 * PARENT IMMEDIATELY....WHAT IF THE ITEM IS ALREADY A GROUP ?????? THE ORIGINAL FUNCTION
 * CANNOT BE USED TO CONVERT A GROUPS POSITION CORRECTLY......S*/

/**
 * my_foo_canvas_item_w2i:
 * @item: A canvas item.
 * @x: X coordinate to convert (input/output value).
 * @y: Y coordinate to convert (input/output value).
 *
 * Converts a coordinate pair from world coordinates to item-relative
 * coordinates.
 **/
void my_foo_canvas_item_w2i (FooCanvasItem *item, double *x, double *y)
{
  g_return_if_fail (FOO_IS_CANVAS_ITEM (item));
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* orginal code... */
  item = item->parent;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
  if (!FOO_IS_CANVAS_GROUP (item))
    item = item->parent;

  while (item) {
    if (FOO_IS_CANVAS_GROUP (item)) {
      *x -= FOO_CANVAS_GROUP (item)->xpos;
      *y -= FOO_CANVAS_GROUP (item)->ypos;
    }

    item = item->parent;
  }

  return ;
}


/**
 * my_foo_canvas_item_i2w:
 * @item: A canvas item.
 * @x: X coordinate to convert (input/output value).
 * @y: Y coordinate to convert (input/output value).
 *
 * Converts a coordinate pair from item-relative coordinates to world
 * coordinates.
 **/
void my_foo_canvas_item_i2w (FooCanvasItem *item, double *x, double *y)
{
  g_return_if_fail (FOO_IS_CANVAS_ITEM (item));
  g_return_if_fail (x != NULL);
  g_return_if_fail (y != NULL);


#ifdef ED_G_NEVER_INCLUDE_THIS_CODE
  /* Original code... */
  item = item->parent;
#endif /* ED_G_NEVER_INCLUDE_THIS_CODE */
  if (!FOO_IS_CANVAS_GROUP (item))
    item = item->parent;

  while (item) {
    if (FOO_IS_CANVAS_GROUP (item)) {
      *x += FOO_CANVAS_GROUP (item)->xpos;
      *y += FOO_CANVAS_GROUP (item)->ypos;
    }

    item = item->parent;
  }

  return ;
}

/* A major problem with foo_canvas_item_move() is that it moves an item by an
 * amount rather than moving it somewhere in the group which is painful for operations
 * like unbumping a column. */
void my_foo_canvas_item_goto(FooCanvasItem *item, double *x, double *y)
{
  double x1, y1, x2, y2 ;
  double dx, dy ;

  if (x || y)
    {
      x1 = y1 = x2 = y2 = 0.0 ;
      dx = dy = 0.0 ;

      foo_canvas_item_get_bounds(item, &x1, &y1, &x2, &y2) ;

      if (x)
        dx = *x - x1 ;
      if (y)
        dy = *y - y1 ;

      foo_canvas_item_move(item, dx, dy) ;
    }

  return ;
}



/* This function returns the visible _world_ coords. Be careful how you use
 * the returned coords as there may not be any of our items at the extreme
 * extents of this range (e.g. x = 0.0 !!). */
void zmapWindowItemGetVisibleWorld(ZMapWindow window,
                                   double *wx1, double *wy1,
                                   double *wx2, double *wy2)
{

  getVisibleCanvas(window, wx1, wy1, wx2, wy2);

  return ;
}




/*
 *                  Internal routines.
 */





/* Get the visible portion of the canvas. */
static void getVisibleCanvas(ZMapWindow window,
                             double *screenx1_out, double *screeny1_out,
                             double *screenx2_out, double *screeny2_out)
{
  GtkAdjustment *v_adjust, *h_adjust ;
  double x1, x2, y1, y2 ;

  /* Work out which part of the canvas is visible currently. */
  v_adjust = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(window->scrolled_window)) ;
  h_adjust = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(window->scrolled_window)) ;

  x1 = h_adjust->value ;
  x2 = x1 + h_adjust->page_size ;

  y1 = v_adjust->value ;
  y2 = y1 + v_adjust->page_size ;

  foo_canvas_window_to_world(window->canvas, x1, y1, screenx1_out, screeny1_out) ;
  foo_canvas_window_to_world(window->canvas, x2, y2, screenx2_out, screeny2_out) ;


  int cx1, cy1, cx2, cy2 ;
  double wx1, wy1, wx2, wy2 ;

  cx1 = (int)x1 ;
  cx2 = (int)x2 ;
  cy1 = (int)y1 ;
  cy2 = (int)y2 ;

  
  wx1 = wy1 = wx2 = wy2 = 0.0 ;

  foo_canvas_c2w(window->canvas, cx1, cy1, &wx1, &wy1) ;

  foo_canvas_w2c(window->canvas, wx1, wy1, &cx1, &cy1) ;

  foo_canvas_c2w(window->canvas, cx2, cy2, &wx2, &wy2) ;

  foo_canvas_w2c(window->canvas, wx2, wy2, &cx2, &cy2) ;




  // more experiments....
  GtkLayout *layout = (GtkLayout *)(window->canvas) ;

  guint width, height ;

  gtk_layout_get_size (layout,
                       &width,
                       &height) ;

  return ;
}

