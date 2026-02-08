# DVD DVDReadAsync tests

These testcases mirror MP4 callsites that do asynchronous reads and then poll
`DVDGetCommandBlockStatus(&fileInfo->cb)` until completion.

