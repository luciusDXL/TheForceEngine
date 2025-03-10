# Setup the root path
root_path="$1"

# TFE does not like tildes in paths
user_path=~

ls -tral $root_path

ls -tral $root_path/TheForceEngine/

ls -tral $root_path/TheForceEngine/Tests/Replays/

ldd $root_path/TheForceEngine/theforceengine

# Run the demo and generate new log file
echo "running tfe"
$root_path/TheForceEngine/theforceengine -gDark -r$root_path/Tests/Replays/base_test.demo --demo_logging --exit_after_replay
echo "Result is "$?
echo "Done running tfe"

if [ -f $root_path/TheForceEngine/theforceengine ]; then
   echo "dump exists"
else
   echo "no dump exists"
fi 

echo "running tfe2"
$root_path/theforceengine -gDark -r$root_path/Tests/Replays/base_test.demo --demo_logging --exit_after_replay
echo "Result is "$?
echo "Done running tfe"


#find / -type d -iname the_force_engine_log.txt 2>/dev/null | xargs cat

top -b -n 1 | head -n 20

# Diff Results of the rest 
result=`diff <(tail -n 1 $user_path/.local/share/TheForceEngine/replay.log) <(tail -n 1 $root_path/TheForceEngine/Tests/Replays/base_replay.log)`

if $result > /dev/null; then
    echo "RESULTS MATCH!"
    exit 0
else
    echo "RESULTS DO NOT MATCH!"
    echo $result
    exit 1
fi

