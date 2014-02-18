/*
 *  GNOME Logs - View and search logs
 *  Copyright (C) 2013  Red Hat, Inc.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"
#include "gl-application.h"

#include <glib/gi18n.h>

#include "gl-categorylist.h"
#include "gl-eventtoolbar.h"
#include "gl-eventview.h"
#include "gl-util.h"
#include "gl-window.h"

typedef struct
{
    GSettings *desktop;
    gchar *monospace_font;
} GlApplicationPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GlApplication, gl_application, GTK_TYPE_APPLICATION)

static const gchar DESKTOP_SCHEMA[] = "org.gnome.desktop.interface";
static const gchar DESKTOP_MONOSPACE_FONT_NAME[] = "monospace-font-name";

static void
on_new_window (GSimpleAction *action,
               GVariant *parameter,
               gpointer user_data)
{
    GtkApplication *application;
    GtkWidget *window;

    application = GTK_APPLICATION (user_data);

    window = gl_window_new (GTK_APPLICATION (application));
    gtk_widget_show (window);
}

static void
on_help (GSimpleAction *action,
         GVariant *parameter,
         gpointer user_data)
{
    GtkApplication *application;
    GtkWindow *parent;
    GError *error = NULL;

    application = GTK_APPLICATION (user_data);
    parent = gtk_application_get_active_window (application);

    /* TODO: Add link to application help instead. */
    gtk_show_uri (gtk_window_get_screen (parent), PACKAGE_URL,
                  GDK_CURRENT_TIME, &error);

    if (error)
    {
        g_debug ("Error while opening help: %s", error->message);
        g_error_free (error);
    }
}

static void
on_about (GSimpleAction *action,
          GVariant *parameter,
          gpointer user_data)
{
    GtkApplication *application;
    GtkWindow *parent;
    static const gchar* artists[] = {
        "Jakub Steiner <jimmac@gmail.com>",
        "Lapo Calamandrei <calamandrei@gmail.com>",
        NULL
    };
    static const gchar* authors[] = {
        "David King <davidk@gnome.org>",
        NULL
    };

    application = GTK_APPLICATION (user_data);
    parent = gtk_application_get_active_window (GTK_APPLICATION (application));
    gtk_show_about_dialog (parent,
                           "authors", authors,
                           "artists", artists,
                           "translator-credits", _("translator-credits"),
                           "comments", _("View and search logs"),
                           "copyright", "Copyright © 2013–2014 Red Hat, Inc.",
                           "license-type", GTK_LICENSE_GPL_3_0,
                           "logo-icon-name", PACKAGE_TARNAME,
                           "program-name", PACKAGE_NAME,
                           "version", PACKAGE_VERSION,
                           "website", PACKAGE_URL, NULL);
}

static void
on_quit (GSimpleAction *action,
         GVariant *parameter,
         gpointer user_data)
{
    GApplication *application;

    application = G_APPLICATION (user_data);
    g_application_quit (application);
}

static void
on_monospace_font_name_changed (GSettings *settings,
                                const gchar *key,
                                GlApplicationPrivate *priv)
{
    gchar *font_name;

    font_name = g_settings_get_string (settings, key);

    if (g_strcmp0 (font_name, priv->monospace_font) != 0)
    {
        GtkCssProvider *provider;
        gchar *css_fragment;

        g_free (priv->monospace_font);
        priv->monospace_font = font_name;

        css_fragment = g_strconcat (".event-monospace { font: ", font_name,
                                    "; }", NULL);
        provider = gtk_css_provider_new ();
        g_signal_connect (provider, "parsing-error",
                          G_CALLBACK (gl_util_on_css_provider_parsing_error),
                          NULL);
        gtk_css_provider_load_from_data (provider, css_fragment, -1, NULL);

        gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                   GTK_STYLE_PROVIDER (provider),
                                                   GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

        g_free (css_fragment);
        g_object_unref (provider);
    }
    else
    {
        g_free (font_name);
    }
}

static GActionEntry actions[] = {
    { "new-window", on_new_window },
    { "help", on_help },
    { "about", on_about },
    { "quit", on_quit }
};

static void
gl_application_startup (GApplication *application)
{
    GtkBuilder *builder;
    GError *error = NULL;
    GMenuModel *appmenu;

    g_action_map_add_action_entries (G_ACTION_MAP (application), actions,
                                     G_N_ELEMENTS (actions), application);

    /* Calls gtk_init() with no arguments. */
    G_APPLICATION_CLASS (gl_application_parent_class)->startup (application);

    builder = gtk_builder_new ();
    gtk_builder_set_translation_domain (builder, GETTEXT_PACKAGE);
    gtk_builder_add_from_resource (builder, "/org/gnome/Logs/appmenu.ui",
                                   &error);

    if (error != NULL)
    {
        g_error ("Unable to get app menu from resource: %s", error->message);
    }

    appmenu = G_MENU_MODEL (gtk_builder_get_object (builder, "appmenu"));
    gtk_application_set_app_menu (GTK_APPLICATION (application), appmenu);

    g_object_unref (builder);

    /* Must register custom types before using them from GtkBuilder. */
    gl_window_get_type ();
    gl_category_list_get_type ();
    gl_event_toolbar_get_type ();
    gl_event_view_get_type ();
}

static void
gl_application_activate (GApplication *application)
{
    GtkWidget *window;
    GlApplicationPrivate *priv;

    window = gl_window_new (GTK_APPLICATION (application));
    gtk_widget_show (window);
    gtk_application_add_accelerator (GTK_APPLICATION (application),
                                     "<Primary>w", "win.close", NULL);
    gtk_application_add_accelerator (GTK_APPLICATION (application),
                                     "<Primary>f", "win.search", NULL);

    priv = gl_application_get_instance_private (GL_APPLICATION (application));

    on_monospace_font_name_changed (priv->desktop, DESKTOP_MONOSPACE_FONT_NAME,
                                    priv);
}

static void
gl_application_finalize (GObject *object)
{
    GlApplication *application;
    GlApplicationPrivate *priv;

    application = GL_APPLICATION (object);
    priv = gl_application_get_instance_private (application);

    g_clear_object (&priv->desktop);
    g_clear_pointer (&priv->monospace_font, g_free);
}

static void
gl_application_init (GlApplication *application)
{
    GlApplicationPrivate *priv;
    gchar *changed_font;

    priv = gl_application_get_instance_private (application);

    priv->monospace_font = NULL;
    priv->desktop = g_settings_new (DESKTOP_SCHEMA);

    changed_font = g_strconcat ("changed::", DESKTOP_MONOSPACE_FONT_NAME, NULL);
    g_signal_connect (priv->desktop, changed_font,
                      G_CALLBACK (on_monospace_font_name_changed),
                      priv);

    g_free (changed_font);
}

static void
gl_application_class_init (GlApplicationClass *klass)
{
    GObjectClass *gobject_class;
    GApplicationClass *app_class;

    gobject_class = G_OBJECT_CLASS (klass);
    gobject_class->finalize = gl_application_finalize;

    app_class = G_APPLICATION_CLASS (klass);
    app_class->activate = gl_application_activate;
    app_class->startup = gl_application_startup;
}

GtkApplication *
gl_application_new (void)
{
    return g_object_new (GL_TYPE_APPLICATION, "application-id",
                         "org.gnome.Logs", NULL);
}
