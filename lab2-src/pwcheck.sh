 #!/bin/bash

#DO NOT REMOVE THE FOLLOWING TWO LINES
git add $0 >> .local.git.out
git commit -a -m "Lab 2 commit" >> .local.git.out
git push >> .local.git.out || echo


#Your code here
COUNT=0
#if statement to check if directory exists
if [[ -e $1 ]]; then
  PASSWORD=$(< $1)
  if [[ ${#PASSWORD} -lt 6 ]] || [[ ${#PASSWORD} -gt 32 ]]; then
    echo "Error: Password length invalid."
  else
    let COUNT=COUNT+${#PASSWORD}

    if egrep -q [#$\+%@] $1; then
      let COUNT=COUNT+5
    fi

    if egrep -q [0-9] $1; then
      let COUNT=COUNT+5
    fi

    if egrep -q [A-Za-z] $1; then
      let COUNT=COUNT+5
    fi

    if egrep -q '([0-9A-Za-z])\1+' $1; then
      let COUNT=COUNT-10
    fi

    if egrep -q [a-z][a-z][a-z]+ $1; then
      let COUNT=COUNT-3
    fi

    if egrep -q [A-Z][A-Z][A-Z]+ $1; then
      let COUNT=COUNT-3
    fi

    if egrep -q [0-9][0-9][0-9]+ $1; then
      let COUNT=COUNT-3
    fi

    echo "Password Score: $COUNT"
  fi

else
  echo "File does not exist"
fi
