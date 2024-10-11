#include <ncurses.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include "stats.h"
#include "ui.h"

// Helper to format numbers in SI units
void ui_format_size(uint64_t bytes, char *out) {
    const char *units[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    int unit = 0;
    double size = (double)bytes;

    while (size >= 1024 && unit < 5) {
        size /= 1024;
        unit++;
    }
    sprintf(out, "%.2f %s", size, units[unit]);
}

// Helper to format throughput in bits per second
void ui_format_throughput(uint64_t bytes, uint64_t time_ns, char *out) {
    const char *units[] = {"bps", "Kbps", "Mbps", "Gbps", "Tbps"};
    int unit = 0;
    double bits_per_sec = (double)(bytes * 8) / (time_ns / 1e9);

    while (bits_per_sec >= 1000 && unit < 4) {
        bits_per_sec /= 1000;
        unit++;
    }
    sprintf(out, "%.2f %s", bits_per_sec, units[unit]);
}

void
display_flow_stats(
    int x, int y,
    struct flowbench_flow *flow)
{
    char sent_size[20], recv_size[20], send_rate[20], recv_rate[20];

    ui_format_size(flow->sent_bytes, sent_size);
    ui_format_size(flow->recv_bytes, recv_size);
    ui_format_throughput(flow->sent_bytes_window.count, STAT_WINDOW_INTERVAL, send_rate);
    ui_format_throughput(flow->recv_bytes_window.count, STAT_WINDOW_INTERVAL, recv_rate);

    mvprintw(y, x, "%s -> %s : %s @ %s TX |  %s @ %s RX", 
             flow->src, flow->dst, sent_size, send_rate, recv_size, recv_rate);

    if (flow->latency_samples) {
        uint64_t avg_latency = flow->total_latency / flow->latency_samples;
        mvprintw(y, x + 90, "%.02F/%.02F/%.02F min/max/avg uS",
                 flow->min_latency / 1000.0F,
                 flow->max_latency / 1000.0F,
                 avg_latency / 1000.0F);
    }
}

void display_summary(struct flowbench_stats *stats) {
    char sent_size[20], recv_size[20], send_rate[20], recv_rate[20];
    uint64_t total_recv_bytes_tp = 0, total_sent_bytes_tp = 0;
    struct flowbench_flow summary = stats->saved;
    struct flowbench_flow *flow = stats->flows;

    DL_FOREACH(stats->flows, flow) {
        summary.sent_msgs += flow->sent_msgs;
        summary.sent_bytes += flow->sent_bytes;
        summary.recv_msgs += flow->recv_msgs;
        summary.recv_bytes += flow->recv_bytes;
        summary.min_latency = flow->min_latency < summary.min_latency || summary.min_latency == 0 ? flow->min_latency : summary.min_latency;
        summary.max_latency = flow->max_latency > summary.max_latency ? flow->max_latency : summary.max_latency;
        summary.total_latency += flow->total_latency;
        summary.latency_samples += flow->latency_samples;

        total_recv_bytes_tp += flow->recv_bytes_window.count;
        total_sent_bytes_tp += flow->sent_bytes_window.count;
    }

    ui_format_size(summary.sent_bytes, sent_size);
    ui_format_size(summary.recv_bytes, recv_size);
    ui_format_throughput(total_recv_bytes_tp, STAT_WINDOW_INTERVAL, recv_rate);
    ui_format_throughput(total_sent_bytes_tp, STAT_WINDOW_INTERVAL, send_rate);

    mvprintw(0, 0, "Summary: Sent: %s (%s), Recv: %s (%s)", sent_size, send_rate, recv_size, recv_rate);

    if (summary.latency_samples) {
        uint64_t avg_latency = summary.total_latency / summary.latency_samples;
        mvprintw(1, 0, "Summary Latency: Min: %.02Fus, Max: %.02Fus, Avg: %.02Fus",
                 summary.min_latency / 1000.0F,
                 summary.max_latency / 1000.0F,
                 avg_latency / 1000.0F);
    }
}

void display_events(struct flowbench_event *event) {
    int row = 5;
    while (event) {
        mvprintw(row++, 0, "Event: %s", event->msg);
        event = event->next;
    }
}

void handle_input(struct flowbench_stats *stats) {
    int ch = getch();
    if (ch == 'x' || ch == 'X') {
        endwin(); // Exit ncurses
        exit(0);
    } else if (ch == 'r' || ch == 'R') {
        // Reset saved stats
        pthread_mutex_lock(&stats->lock);
        memset(&stats->saved, 0, sizeof(stats->saved));
        pthread_mutex_unlock(&stats->lock);
    }
}

void
update_screen(struct flowbench_stats *stats)
{
    struct flowbench_flow *flow;

    int y;

    pthread_mutex_lock(&stats->lock);

    clear();
    
    y = 4;
    DL_FOREACH(stats->flows, flow) {
        display_flow_stats(4, y, flow);
        y++;
    }

    display_summary(stats);
    //display_events(stats->events);

    pthread_mutex_unlock(&stats->lock);

    refresh();
}

void
ui_init(int interval_ms)
{
    initscr();
    cbreak();
    noecho();
    timeout(interval_ms);
};

void
ui_update(struct flowbench_stats *stats)
{
    update_screen(stats);
    handle_input(stats);
}

void
ui_cleanup()
{
    endwin();
}

static void
ui_print_flow(struct flowbench_flow *flow, uint64_t duration)
{
    char sent_size[20], recv_size[20], send_rate[20], recv_rate[20];

    ui_format_size(flow->sent_bytes, sent_size);
    ui_format_size(flow->recv_bytes, recv_size);
    ui_format_throughput(flow->sent_bytes, duration, send_rate);
    ui_format_throughput(flow->recv_bytes, duration, recv_rate);

    printf("Flow: Sent: %s (%s), Recv: %s (%s)", sent_size, send_rate, recv_size, recv_rate);

    if (flow->recv_msgs > 0) {
        uint64_t avg_latency = flow->total_latency / flow->recv_msgs;
        printf(" | Latency: Min: %luns, Max: %luns, Avg: %luns",
                 flow->min_latency, flow->max_latency, avg_latency);
    }

    printf("\n");
}

void
ui_print_stats(
    struct flowbench_stats *stats, uint64_t duration)
{
    struct flowbench_flow *flow;

    pthread_mutex_lock(&stats->lock);

    DL_FOREACH(stats->flows, flow) {
        ui_print_flow(flow, duration);
    }

    pthread_mutex_unlock(&stats->lock);

}