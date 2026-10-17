# Sourced by the check scripts (from the repository root, after their cd):
# the one source of their parallelism. JOBS is how many compiles, models or
# checks run at once; the default is the project's -j4, not the machine's core
# count, since the laptop overheats (W418). JOBS=N tools/x.sh overrides it.
JOBS=${JOBS:-4}
