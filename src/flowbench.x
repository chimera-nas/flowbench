/*
 * SPDX-FileCopyrightText: 2025 Ben Jarvis
 * SPDX-License-Identifier: LGPL-2.1-only
 */

struct Ping {
    zcopaque        data;
};

struct Pong {
    zcopaque        data;
};

struct Datagram {
    zcopaque data;
};

program FLOWBENCH_PROGRAM {
    version FLOWBENCH_V1 {
        Pong pingpong(Ping) = 1;
        void datagram(Datagram) = 2;
    } = 1;
} = 42;