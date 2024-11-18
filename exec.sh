# Add -lpq if the argument is "server"
if [ "$1" = "server" ]; then
  gcc -o "$1" "$1".c -lpq && ./"$1"
else
  gcc -o "$1" "$1".c && ./"$1"
fi