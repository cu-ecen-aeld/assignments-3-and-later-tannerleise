#!/bin/bash
#-ne denotes !=, You can use either

#White space matters in variable declaration
filesdir=$1;
searchstr=$2;

#White space matters in an if statement as well!
if [ $# -ne 2 ]
then
	echo "Need to enter 2 Arguments!"
	exit 1
fi

if [ ! -d "$filesdir" ]
then
	echo "Your first argument needs to be a directory!"
	exit 1
fi

#The first part of the command is saying find any file in filesdir (where tyoe -f specifies we want only files)
#We then pipe all these found files into wc (word count) which will count according to the rules we give it. In this case, we give it -l which is the number of new lines.
numfiles=$(find "$filesdir" -type f | wc -l)
#The first part of the command is saying to recursively look for our searchstring within the specified directory
#Pipe that to wc -l to get the actual count
nummatches=$(grep -r "$searchstr" "$filesdir" | wc -l)

echo "The number of files are $numfiles and the number of matching lines are $nummatches"
