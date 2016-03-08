How to run the program:

1. Replace the ‘cs1550.c’ file in fuse-2.7.0/example/. Copy both shell scripts ‘refreshDisk.sh’ and ‘unmount.sh’ under ‘example’ directory. Run ‘sh refreshDisk.sh’ to initialize the system (Assigning 5 MB capacity to .disk file and creating the .disk file)

2. Create a test mounting point called ‘testmount’ under ‘example’ directory. run 

3. Have two terminal windows up.

4. In the first window, cd into ‘example’ directory. run ‘./cs1550 -d test mount’

5. In the second window, cd into ‘test mount’. run commands like ‘ls -a’ and ‘mkdir temp’

6. New directories that are created should be visible by typing unix command ‘ls’

7. ‘rmdir’ is not required in the project requirement thus deleting a directory is not available

8. ‘echo xxx > filename.extension’ can be used to test write functionality

9. ‘cat filename.extension’ can be used to test read functionality

10. ‘rm filename.extension’ can be used to test unlink functionality

11. Run ‘unmount.sh’ unmount the file system we just used when we are done or need to make changes to the program.

12. Run ‘refresh.sh’ to reinitialize the .disk file
