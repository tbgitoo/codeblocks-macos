#include <gtk/gtk.h>

static void hello_world(GtkWidget *widget, gpointer data)
{
    GtkWindow *parent = GTK_WINDOW(data);

    GtkAlertDialog *dialog =
        gtk_alert_dialog_new("Hello World!");

    gtk_alert_dialog_show(dialog, parent);
    g_object_unref(dialog);
}

static void activate(GtkApplication *app, gpointer user_data)
{
    GtkWidget *window;
    GtkWidget *box;
    GtkWidget *info_button;
    GtkWidget *close_button;

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Hello World");
    gtk_window_set_default_size(GTK_WINDOW(window), 300, 160);

    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_margin_top(box, 12);
    gtk_widget_set_margin_bottom(box, 12);
    gtk_widget_set_margin_start(box, 12);
    gtk_widget_set_margin_end(box, 12);
    gtk_window_set_child(GTK_WINDOW(window), box);

    info_button = gtk_button_new_with_label("Hello World!");
    g_signal_connect(info_button,
                     "clicked",
                     G_CALLBACK(hello_world),
                     window);
    gtk_box_append(GTK_BOX(box), info_button);

    close_button = gtk_button_new_with_label("Close");
    g_signal_connect_swapped(close_button,
                             "clicked",
                             G_CALLBACK(gtk_window_destroy),
                             window);
    gtk_box_append(GTK_BOX(box), close_button);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char *argv[])
{
    GtkApplication *app;
    int status;

    app = gtk_application_new("org.codeblocks.gtk4example",
                              G_APPLICATION_DEFAULT_FLAGS);

    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    status = g_application_run(G_APPLICATION(app), argc, argv);

    g_object_unref(app);

    return status;
}
