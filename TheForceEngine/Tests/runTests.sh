# TFE Does not like ~ so lets expand it
user_path=~

# Run the demo and generate new log file
$user_path/tfe/theforceengine -gDark -r$user_path/tfe/TheForceEngine/Tests/Replays/base_test.demo --demo_logging --exit_after_replay

# Diff Results of the rest 
result=`diff <(tail -n 1 $user_path/.local/share/TheForceEngine/replay.log) <(tail -n 1 $user_path/tfe/TheForceEngine/Tests/Replays/base_replay.log)`

if $result > /dev/null; then
    exit 0
else
    echo $result
    exit 1
fi

