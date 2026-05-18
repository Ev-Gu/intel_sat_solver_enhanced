# LSU Manager + Totalizer Test Suite

קובץ בדיקה לאימות קוד ה-LSUManager עם Totalizer

## מה קובץ זה בודק?

### Test Case 1: LSUManager בעיה פשוטה
- **בעיה:**
  - Hard clauses: (1 ∨ 2), (-1 ∨ 3)
  - Soft clauses: (-2) weight=1, (-3) weight=1
  
- **מה בודקים:**
  - ✓ יצירת Topor solver
  - ✓ הוספת hard clauses
  - ✓ יצירת relaxation variables
  - ✓ הוספת soft clauses בצורה (C ∨ r_i)
  - ✓ קריאה ראשונית ל-Solve()
  - ✓ קריאה ל-GetLitValue() לכל משתנה
  - ✓ בדיקת weight של relaxation variables
  - ✓ יצירת LSUManager instance

### Test Case 2: בניית Totalizer
- **מה בודקים:**
  - ✓ יצירת Totalizer עם relaxation variables
  - ✓ בניית encoding clauses
  - ✓ קריאה ל-get_clauses()

## איך להרוץ?

### Windows:
```cmd
build_test.bat
test_lsu_totalizer.exe
```

### Linux/Mac:
```bash
chmod +x build_test.sh
./build_test.sh
./test_lsu_totalizer
```

### Manual compile (כל מערכת):
```bash
g++ -std=c++20 -DSKIP_ZLIB -Wall -g -O0 -o test_lsu_totalizer \
  test_lsu_totalizer.cpp Topor.cc Topi.cc TopiAsg.cc TopiBacktrack.cc \
  TopiBcp.cc TopiBitCompression.cc TopiCompression.cc TopiConflictAnalysis.cc \
  TopiDebugPrinting.cc TopiDecision.cc TopiInprocess.cc TopiRestart.cc \
  TopiStatistics.cc TopiVarScores.cc TopiWL.cc -lpthread
```

## תוצאות צפויות

אם הכל עובד:
```
=== LSU Manager + Totalizer Test ===

Test Case 1: Simple MaxSAT Problem
Hard clauses:
  (1 v 2)
  (-1 v 3)
Soft clauses with weights:
  (-2) weight=1
  (-3) weight=1

Adding hard clauses...
Preparing soft clauses with relaxation variables...
Soft clause 1: (-2 v r4)
Soft clause 2: (-3 v r5)

Initial Solve (before LSU)...
Result: SAT
Solution found:
  Variable assignments:
    Var 1 = [TRUE/FALSE]
    ...
  Relaxation variables status:
    r4 = [TRUE/FALSE]
    r5 = [TRUE/FALSE]
  Current cost (violations): [0/1/2]

=== Creating LSUManager ===
LSUManager created successfully!
Number of relaxation variables: 2

LSUManager ready for optimization.
Best weight before optimization: -1

=== Test Case 1 PASSED ===

Test Case 2: Totalizer Creation
Creating Totalizer with 3 relaxation variables...
Totalizer built successfully!
Number of encoding clauses: [X]
Sample clause details:
  Clause with [N] literals
  ...

=== Test Case 2 PASSED ===

=== ALL TESTS COMPLETED SUCCESSFULLY ===
```

## איפה דבוגים?

1. **אם ישנן שגיאות compilation:**
   - בדוק שכל הקבצים ה-cpp/cc קיימים
   - וודא שיש לך C++20 compiler
   - בדוק את ה-include paths

2. **אם התוצאות לא צפויות:**
   - הדפס את המודל אחרי כל Solve()
   - בדוק את ערכי relaxation variables
   - וודא שה-clauses התווספו נכון

3. **אם יש crash:**
   - הרץ עם debugger: `gdb test_lsu_totalizer`
   - או `lldb test_lsu_totalizer` ב-Mac

## צעדים הבאים

לאחר שהבדיקה עובדת:
1. הוסף עוד test cases עם בעיות מורכבות יותר
2. בדוק את ה-optimization loop של LSUManager
3. וודא output ("o " lines) תקין
4. בדוק integration עם Main.cc
