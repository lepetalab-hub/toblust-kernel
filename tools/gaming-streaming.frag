# Scheduler dan timer: respons lebih rapat untuk game
enable CONFIG_HZ_1000
enable CONFIG_UCLAMP_TASK
enable CONFIG_UCLAMP_TASK_GROUP
disable CONFIG_SCHED_DEBUG

# Jaringan untuk streaming: BBR + fq
enable CONFIG_TCP_CONG_BBR
enable CONFIG_NET_SCH_FQ

# MODVERSIONS dimatikan penuh (bukan stub)
disable CONFIG_MODVERSIONS
