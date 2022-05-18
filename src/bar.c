/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <gtk/gtk.h>
#include <glib-unix.h>
#include <gtk-layer-shell.h>
#include "sfwbar.h"
#include "config.h"

GHashTable *bar_list;

GtkWidget *bar_get_from_widget ( GtkWidget *widget )
{

  return gtk_widget_get_ancestor(widget,GTK_TYPE_WINDOW);
}

GtkWidget *bar_get_by_name ( gchar *name )
{
  if(!bar_list)
    return NULL;
  if(!name)
    name = "sfwbar";

  return g_hash_table_lookup(bar_list,name);
}

struct layout_widget *bar_grid_by_name ( gchar *addr )
{
  GtkWidget *bar, *box;
  struct layout_widget *lw = NULL;
  gchar *grid, *ptr, *name;

  if(!addr)
    addr = "sfwbar";

  ptr = strchr(addr,':');

  if(ptr)
  {
    grid = ptr + 1;
    name = g_strndup(addr,ptr-addr-1);
  }
  else
  {
    grid = NULL;
    name = g_strdup(addr);
  }

  bar = bar_get_by_name (name);
  if(!bar)
    bar = GTK_WIDGET(bar_new(name));
  g_free(name);

  if(grid)
    lw = g_object_get_data(G_OBJECT(bar),grid);

  if(lw)
    return lw;

  lw = layout_widget_new();
  lw->wtype = G_TOKEN_GRID;
  lw->widget = gtk_grid_new();
  gtk_widget_set_name(lw->widget,"layout");

  box = gtk_bin_get_child(GTK_BIN(bar));
  if(grid && !g_ascii_strcasecmp(grid,"center"))
  {
    gtk_box_set_center_widget(GTK_BOX(box),lw->widget);
    g_object_set_data(G_OBJECT(bar),"center",lw);
  }
  else if(grid && !g_ascii_strcasecmp(grid,"right"))
  {
    gtk_box_pack_end(GTK_BOX(box),lw->widget,TRUE,TRUE,0);
    g_object_set_data(G_OBJECT(bar),"right",lw);
  }
  else
  {
    gtk_box_pack_start(GTK_BOX(box),lw->widget,TRUE,TRUE,0);
    g_object_set_data(G_OBJECT(bar),"left",lw);
  }
  return lw;
}

gboolean bar_hide_event ( struct json_object *obj )
{
  gchar *mode, state;
  void *key,*bar;
  struct json_object *visible;
  static gchar pstate = 's';
  GHashTableIter iter;

  if ( obj )
  {
    mode = json_string_by_name(obj,"mode");
    if(mode)
    {
      pstate = *mode;
      g_free(mode);
    }
    state = pstate;

    if(json_object_object_get_ex(obj,"visible_by_modifier",&visible))
      if(json_object_get_boolean(visible))
        state = 's';
  }
  else
    if( pstate == 's' )
      pstate = state = 'h';
    else
      pstate = state = 's';

  g_hash_table_iter_init(&iter,bar_list);
  while(g_hash_table_iter_next(&iter,&key,&bar))
    if(state=='h')
      gtk_widget_hide(GTK_WIDGET(bar));
    else
      if(!gtk_widget_is_visible(GTK_WIDGET(bar)))
        bar_update_monitor(GTK_WINDOW(bar));
  return TRUE;
}

gint bar_get_toplevel_dir ( GtkWidget *widget )
{
  GtkWidget *toplevel;
  gint toplevel_dir;

  if(!widget)
    return GTK_POS_RIGHT;

  toplevel = bar_get_from_widget(widget);

  if(!toplevel)
    return GTK_POS_RIGHT;

  gtk_widget_style_get(toplevel, "direction",&toplevel_dir,NULL);
  return toplevel_dir;
}

void bar_set_layer ( gchar *layer_str, gchar *addr )
{
  GtkLayerShellLayer layer = GTK_LAYER_SHELL_LAYER_TOP;
  GtkWidget *bar;

  bar = bar_get_by_name(addr);
  if(!bar)
    return;

  if(!g_ascii_strcasecmp(layer_str,"background"))
    layer = GTK_LAYER_SHELL_LAYER_BACKGROUND;
  if(!g_ascii_strcasecmp(layer_str,"bottom"))
    layer = GTK_LAYER_SHELL_LAYER_BOTTOM;
  if(!g_ascii_strcasecmp(layer_str,"top"))
    layer = GTK_LAYER_SHELL_LAYER_TOP;
  if(!g_ascii_strcasecmp(layer_str,"overlay"))
    layer = GTK_LAYER_SHELL_LAYER_OVERLAY;

  gtk_layer_set_layer(GTK_WINDOW(bar),layer);
}

void bar_set_exclusive_zone ( gchar *zone, gchar *addr )
{
  gint exclusive_zone;
  GtkWidget *bar;

  bar = bar_get_by_name(addr);
  if(!bar)
    return;

  if(!g_ascii_strcasecmp(zone,"auto"))
  {
    exclusive_zone = -2;
    gtk_layer_auto_exclusive_zone_enable (GTK_WINDOW(bar));
  }
  else
  {
    exclusive_zone = MAX(-1,g_ascii_strtoll(zone,NULL,10));
    gtk_layer_set_exclusive_zone (GTK_WINDOW(bar), exclusive_zone );
  }
}

gchar *bar_get_output ( GtkWidget *widget )
{
  GdkWindow *win;
  GtkWidget *toplevel;

  toplevel = bar_get_from_widget(widget);

  win = gtk_widget_get_window(GTK_WIDGET(toplevel));
  return g_object_get_data( G_OBJECT(gdk_display_get_monitor_at_window(
        gdk_window_get_display(win),win)), "xdg_name" );
}

void bar_update_monitor ( GtkWindow *win )
{
  GdkDisplay *gdisp;
  GdkMonitor *gmon,*match;
  gint nmon,i;
  gchar *name, *monitor;

  if(!win)
    return;

  monitor = g_object_get_data(G_OBJECT(win),"monitor");

  gdisp = gdk_display_get_default();
  match = gdk_display_get_primary_monitor(gdisp);

  if(monitor)
  {
    nmon = gdk_display_get_n_monitors(gdisp);
    for(i=0;i<nmon;i++)
    {
      gmon = gdk_display_get_monitor(gdisp,i);
      name = g_object_get_data(G_OBJECT(gmon),"xdg_name");
      if(name && !g_strcmp0(name,monitor))
        match = gmon;
    }
  }

  gtk_widget_hide(GTK_WIDGET(win));
  gtk_layer_set_monitor(win, match);
  gtk_widget_show(GTK_WIDGET(win));
}

void bar_set_monitor ( gchar *mon_name, gchar *addr )
{
  gchar *monitor;
  GtkWidget *bar;

  bar = bar_get_by_name(addr);
  if(!bar)
    return;
  monitor = g_object_get_data(G_OBJECT(bar),"monitor");

  if(!monitor || g_ascii_strcasecmp(monitor, mon_name))
  {
    g_free(monitor);
    g_object_set_data(G_OBJECT(bar),"monitor", g_strdup(mon_name));
    bar_update_monitor(GTK_WINDOW(bar));
  }
}

void bar_monitor_added_cb ( GdkDisplay *gdisp, GdkMonitor *gmon )
{
  GHashTableIter iter;
  void *key, *bar;
  wayland_output_new(gmon);

  g_hash_table_iter_init(&iter,bar_list);
  while(g_hash_table_iter_next(&iter,&key,&bar))
    bar_update_monitor(bar);
}

void bar_monitor_removed_cb ( GdkDisplay *gdisp, GdkMonitor *gmon )
{
  GHashTableIter iter;
  void *key, *bar;
  wayland_output_destroy(gmon);

  g_hash_table_iter_init(&iter,bar_list);
  while(g_hash_table_iter_next(&iter,&key,&bar))
    bar_update_monitor(bar);
}

void bar_set_size ( gchar *size, gchar *addr )
{
  gdouble number;
  gchar *end;
  GdkRectangle rect;
  GdkWindow *win;
  gint toplevel_dir;
  GtkWindow *bar;

  bar = GTK_WINDOW(bar_get_by_name(addr));
  if(!bar)
    return;
  number = g_ascii_strtod(size, &end);
  win = gtk_widget_get_window(GTK_WIDGET(bar));
  gdk_monitor_get_geometry( gdk_display_get_monitor_at_window(
      gdk_window_get_display(win),win), &rect );
  toplevel_dir = bar_get_toplevel_dir(GTK_WIDGET(bar));

  if ( toplevel_dir == GTK_POS_BOTTOM || toplevel_dir == GTK_POS_TOP )
  {
    if(*end == '%')
      number = number * rect.width / 100;
    if ( number >= rect.width )
    {
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_LEFT, TRUE );
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_RIGHT, TRUE );
    }
    else
    {
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_LEFT, FALSE );
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_RIGHT, FALSE );
      gtk_widget_set_size_request(GTK_WIDGET(bar),(gint)number,-1);
    }
  }
  else
  {
    if(*end == '%')
      number = number * rect.height / 100;
    if ( number >= rect.height )
    {
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_TOP, TRUE );
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_BOTTOM, TRUE );
    }
    else
    {
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_TOP, FALSE );
      gtk_layer_set_anchor (bar,GTK_LAYER_SHELL_EDGE_BOTTOM, FALSE );
      gtk_widget_set_size_request(GTK_WIDGET(bar),-1,(gint)number);
    }
  }
}

GtkWindow *bar_new ( gchar *name )
{
  GtkWindow *win;
  GtkWidget *box;
  gint toplevel_dir;

  win = (GtkWindow *)gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_layer_init_for_window (win);
  gtk_widget_set_name(GTK_WIDGET(win),name);
  gtk_layer_auto_exclusive_zone_enable ( win );
  gtk_layer_set_keyboard_interactivity(win,FALSE);
  gtk_layer_set_layer(win,GTK_LAYER_SHELL_LAYER_TOP);

  gtk_widget_style_get(GTK_WIDGET(win),"direction",&toplevel_dir,NULL);
  gtk_layer_set_anchor (win,GTK_LAYER_SHELL_EDGE_LEFT,
      !(toplevel_dir==GTK_POS_RIGHT));
  gtk_layer_set_anchor (win,GTK_LAYER_SHELL_EDGE_RIGHT,
      !(toplevel_dir==GTK_POS_LEFT));
  gtk_layer_set_anchor (win,GTK_LAYER_SHELL_EDGE_BOTTOM,
      !(toplevel_dir==GTK_POS_TOP));
  gtk_layer_set_anchor (win,GTK_LAYER_SHELL_EDGE_TOP,
      !(toplevel_dir==GTK_POS_BOTTOM));

  box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
  if(toplevel_dir == GTK_POS_LEFT || toplevel_dir == GTK_POS_RIGHT)
    gtk_orientable_set_orientation(GTK_ORIENTABLE(box),
        GTK_ORIENTATION_VERTICAL);
  gtk_container_add(GTK_CONTAINER(win), box);

  if(!bar_list)
    bar_list = g_hash_table_new((GHashFunc)str_nhash,(GEqualFunc)str_nequal);

  g_hash_table_insert(bar_list, name, win);

  return win;
}