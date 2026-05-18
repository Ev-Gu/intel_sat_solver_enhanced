# הוראות הרצת בדיקה LSU

## שלב 1: בדוק אם קיים executable

```cmd
cd "c:\Users\97254\OneDrive\שולחן העבודה\תומאס\לימודים\שנה א\קוד\פרוייקט גמר\MAX_SAT\intel_sat_solver_enhanced_totalizer\intel_sat_solver_enhanced"
dir Main.exe
dir intel_sat*.exe
dir topor*.exe
```

## שלב 2: אם קיים Main.exe - בדוק MaxSAT:

```cmd
Main.exe -M 1 regression_instances/test_maxsat_lsu.cnf
```

**בדוק output:**
- יצא "o X" (עלות) בכל איטרציה
- בסוף "s SATISFIABLE" או "s UNSATISFIABLE"

## שלב 3: אם אין executable - בנה עם Visual Studio

```cmd
cd "c:\Users\97254\OneDrive\שולחן העבודה\תומאס\לימודים\שנה א\קוד\פרוייקט גמר\MAX_SAT\intel_sat_solver_enhanced_totalizer\intel_sat_solver_enhanced"
```

ואז פתח את `topor.sln` בVisual Studio

קליק על "Build" → "Build Solution"

## שלב 4: הרץ את qubile בדיקה:

```cmd
Release\Main.exe -M 1 regression_instances/test_maxsat_lsu.cnf
```

או

```cmd
Debug\Main.exe -M 1 regression_instances/test_maxsat_lsu.cnf
```

## ערך צפוי Output:

```
c running in MaxSAT mode
c reading file regression_instances/test_maxsat_lsu.cnf
o 1
o 0
s SATISFIABLE
```

זה אומר:
- סיבוב ראשון: cost = 1 (clause עם weight=1 הופר)
- סיבוק שני: cost = 0 (כל clauses מרוצים)
- SAT - בדיקה בוחרת
