#include <stdio.h>
#include <sys/sysinfo.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

int year, month, day;

long uptime_in_seconds(void) {
    struct sysinfo info;
    sysinfo(&info);
    return info.uptime;
}

void set_date(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    year  = t->tm_year + 1900;
    month = t->tm_mon + 1;
    day   = t->tm_mday;
}

void write_to_file() {
    long uptime = uptime_in_seconds();
    FILE *f = fopen("time.txt", "r");
    char line[256];
    long old_uptime = -1;

    if (f) {
        while (fgets(line, sizeof(line), f)) {
            int y, m, d;
            long up;
            if (sscanf(line, "%d-%d-%d-%ld", &y, &m, &d, &up) == 4) {
                if (y == year && m == month && d == day) {
                    old_uptime = up;
                    break;
                }
            }
        }
        fclose(f);
    }

    if (old_uptime == -1) {
        f = fopen("time.txt", "a");
        if (f) {
            fprintf(f, "%d-%d-%d-%ld\n", year, month, day, uptime);
            fclose(f);
        }
    } else if (uptime > old_uptime) {
        f = fopen("time.txt", "r");
        FILE *tmp = fopen("time.txt.tmp", "w");
        if (f && tmp) {
            while (fgets(line, sizeof(line), f)) {
                int y, m, d;
                if (sscanf(line, "%d-%d-%d", &y, &m, &d) == 3 &&
                    y == year && m == month && d == day) {
                    fprintf(tmp, "%d-%d-%d-%ld\n", year, month, day, uptime);
                } else {
                    fputs(line, tmp);
                }
            }
        }
        if (f) fclose(f);
        if (tmp) fclose(tmp);
        rename("time.txt.tmp", "time.txt");
    }
}

long read_from_file(const char *filename, const char *date) {
    FILE *fp = fopen(filename, "r");
    if (!fp) { perror("fopen"); return -1; }

    char line[256];
    long result = -1;

    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, date, strlen(date)) == 0) {
            char *p = line + strlen(date);
            if (*p == '-') {
                result = strtol(p + 1, NULL, 10);
            }
            break;
        }
    }

    fclose(fp);
    return result;
}

void print_date(int year, int month, int day, char *out, size_t outsz) {
    char date[32];
    snprintf(date, sizeof(date), "%04d-%02d-%02d", year, month, day);

    long n = read_from_file("time.txt", date);

    if (n < 0) {
        snprintf(out, outsz, "0s");
    } else if (n < 60) {
        snprintf(out, outsz, "%ld s", n);
    } else if (n < 3600) {
        snprintf(out, outsz, "%ld min", n / 60);
    } else {
        snprintf(out, outsz, "%ld hour, %ld min", n / 3600, (n % 3600) / 60);
    }
}

GtkWidget *lbl_today;
GtkWidget *list_history;

void format_seconds(long n, char *out, size_t outsz) {
    if (n < 60) {
        snprintf(out, outsz, "%ld s", n);
    } else if (n < 3600) {
        snprintf(out, outsz, "%ld min", n / 60);
    } else {
        snprintf(out, outsz, "%ld hour, %ld min", n / 3600, (n % 3600) / 60);
    }
}

void load_history(void) {
    GList *kids = gtk_container_get_children(GTK_CONTAINER(list_history));
    for (GList *i = kids; i; i = i->next)
        gtk_widget_destroy(GTK_WIDGET(i->data));
    g_list_free(kids);

    FILE *f = fopen("time.txt", "r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int y, m, d;
        long up;
        if (sscanf(line, "%d-%d-%d-%ld", &y, &m, &d, &up) != 4) continue;

        char date[32];
        snprintf(date, sizeof(date), "%04d-%02d-%02d", y, m, d);

        char dur[64];
        format_seconds(up, dur, sizeof(dur));

        char row[128];
        snprintf(row, sizeof(row), "%s    %s", date, dur);

        GtkWidget *lb = gtk_label_new(row);
        gtk_widget_set_halign(lb, GTK_ALIGN_START);
        gtk_widget_set_margin_start(lb, 8);
        gtk_widget_set_margin_end(lb, 8);
        gtk_widget_set_margin_top(lb, 4);
        gtk_widget_set_margin_bottom(lb, 4);
        gtk_list_box_insert(GTK_LIST_BOX(list_history), lb, -1);
        gtk_widget_show(lb);
    }
    fclose(f);
}

void refresh(void) {
    set_date();
    write_to_file();

    char buf[64];
    print_date(year, month, day, buf, sizeof(buf));
    gtk_label_set_text(GTK_LABEL(lbl_today), buf);

    load_history();
}

gboolean on_tick(gpointer data) {
    refresh();
    return TRUE;
}

void on_click(GtkButton *b, gpointer data) {
    refresh();
}

void activate(GtkApplication *app, gpointer data) {
    GtkWidget *win = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "Screen Time");
    gtk_window_set_default_size(GTK_WINDOW(win), 320, 420);

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(box), 16);
    gtk_container_add(GTK_CONTAINER(win), box);

    lbl_today = gtk_label_new("...");
    gtk_box_pack_start(GTK_BOX(box), lbl_today, FALSE, FALSE, 0);

    GtkWidget *btn = gtk_button_new_with_label("Refresh");
    g_signal_connect(btn, "clicked", G_CALLBACK(on_click), NULL);
    gtk_box_pack_start(GTK_BOX(box), btn, FALSE, FALSE, 0);

    list_history = gtk_list_box_new();
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), list_history);
    gtk_box_pack_start(GTK_BOX(box), scroll, TRUE, TRUE, 0);

    gtk_widget_show_all(win);

    refresh();
    g_timeout_add_seconds(1, on_tick, NULL);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.example.screentime", G_APPLICATION_FLAGS_NONE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
