# Setup theforceengine binary directory 
root_path=""

# If unset try to look in a common folder
if [[ ! -s "$root_path" || ! -f "$root_path/theforceengine" ]]; then
    root_path="$(dirname "$(realpath "$0")")"/../../
    if [ -f $root_path/theforceengine ]; then
	echo "Found theforceengine in path $root_path"
    else
	echo "WARNING: Cannot find theforceengine binary. Will try one folder up"
	root_path="$(dirname "$(realpath "$0")")"/../../../
	if [ -f $root_path/theforceengine ]; then
	    echo "Found theforceengine in path $root_path"
	else
	    echo "ERROR: Cannot find theforceengine binary. Please set it in the script"
	    exit 1
	fi
    fi
fi    

# Directory where your TFE logs are kept - needed for logs
user_doc_path=""

if [[ ! -s "$user_doc_path" || ! -d "$user_doc_path" ]]; then
    # TFE does not like tildes in paths    
    user_root=$(realpath ~)
    user_doc_path=$user_root/.local/share/TheForceEngine
    if [ -d $user_doc_path ]; then
	echo "Found the user directory in $user_doc_path"
    else
	echo "WARNING: Cannot find the user tfe directory $user_doc_path. Hopefully it will be created by the executable"
    fi
fi

# Path to the demo file
demo_path=""

if [[ ! -s "$demo_path" || ! -f "$demo_path" ]]; then
    demo_path="$(dirname "$(realpath "$0")")"/Replays/base_test.demo
    if [ -f $demo_path ]; then
	echo "Found the demo at $demo_path"
    else
	echo "ERROR: Cannot find the demo file $demo_path. It should be in the TheForceEngine/Tests/Replays folder"
	exit 1
    fi
fi

# Path to the demo log file
demo_log_path=""

if [[ ! -s "$demo_log_path" || ! -f "$demo_log_path" ]]; then
    demo_log_path="$(dirname "$(realpath "$0")")"/Replays/base_replay_log.txt
    if [ -f $demo_log_path ]; then
	echo "Found the demo log at $demo_log_path"
    else
	echo "ERROR: Cannot find the demo log file $demo_log_path. It should be in the TheForceEngine/Tests/Replays folder"
	exit 1
    fi
fi

# Run the demo and generate new log file
pushd $root_path
echo "Running TFE test..."
echo "Executing Command $root_path/theforceengine -gDark -r$demo_path --demo_logging --exit_after_replay ....."
$root_path/theforceengine -gDark -r$demo_path --demo_logging --exit_after_replay
result=$?
echo "Done running test. Result is $result"

if [ $result != "0" ]; then
    echo "ERROR: TFE failed to run. Please look at the logs hopefully generated in $user_doc_path"
    exit 1
fi

# Diff Log Results of the test replay.log and the base one.

if [ ! -f $user_doc_path/replay.log ]; then
    echo "ERROR: Missing $user_doc_path/replay.log - No log generated?"
    exit 1
fi

if [ ! -f $demo_log_path ]; then
    echo "ERROR: Missing $demo_log_path - did you not checkout the latest code?"
    exit 1
fi    

result=`diff <(tail -n 1 $user_doc_path/replay.log) <(tail -n 1 $demo_log_path)`

if $result > /dev/null; then
    echo "TEST SUCCEEDED RESULTS MATCH!"
    exit 0
else
    echo "ERROR: RESULTS DO NOT MATCH!"
    echo $result
    exit 1
fi
