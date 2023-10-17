For using visual leak detector you need to:

1. add vld includes to project  
for msvc:  
in project property page:  
 ``C/C++ -> General -> add Additional Include Directories``  
(for example : `"VisualLeakDetector\include"`)

2. link vld lib to project  
for msvc:  
in project property page:  
 ``Linker -> General -> add Additional Library Directories(according to target's version choose win32 or win64 folder)``  
(for example : `"VisualLeakDetector\lib\Win32"`)

3. copy to project folder (`near *.exe file`) bin/(win32/win64) folder contents 

4. add to main.cpp 
```cpp 
#include "vld.h"
```

5. launch build as you usualy do

6. after closing the program, you will see the leak detector's log in output field  
```
No memory leaks detected.
Visual Leak Detector is now exiting.
```

OR

```
WARNING: Visual Leak Detector detected memory leaks!
```

7. see logs, check call stack of memory leaks, find solution

8. you are breathtaking