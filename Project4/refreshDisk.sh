rm .disk
dd bs=1K count=5K if=/dev/zero of=.disk
echo "refresh complete!"
