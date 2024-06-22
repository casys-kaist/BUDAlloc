#!/usr/bin/env bpftrace

BEGIN
{
	printf("%-12s %-7s %-16s %-6s %7s\n", "TIME(ms)", "DISK", "COMM", "PID", "LAT(ms)");
}

kprobe:bpf_set_page_table,
kprobe:bpf_unset_page_table
{
	@start[pid] = nsecs;
}

kretprobe:bpf_set_page_table,
kretprobe:bpf_unset_page_table
{
	@stats[probe] = stats((nsecs - @start[pid]));
	delete(@start[pid]);
}

END
{
	clear(@start);
}
