# ShadowRun

## What it does
This PoC takes the user input, creates a process with that input, renames the ::$DATA stream of the running executable and then uses SetFileInformationByHandle with the FILE_DISPOSITION_FLAG_DELETE and FILE_DISPOSITION_FLAG_POSIX_SEMANTICS flags to delete it while it will continue its execution

Once the process exits or the app is closed, the file will be written back and get its file timestamp back.
