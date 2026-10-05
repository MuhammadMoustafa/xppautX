# The Data Browser

The Data Browser (DB) lets you look at the numbers a run produced, save
them to a file and otherwise manipulate them; it is a very primitive
spread sheet. Once you have computed a trajectory, use the Data Browser to
look at the data.

**In web2** the DB is the **Data** tab (`ui/TableView.tsx`): a virtualized
table, so there is no window to iconify, resize or "shake" to make the
data show up (all known bugs of the X11 DB window). Across the top is a
menu of commands and then there follows a list of titles for the
variables, time and the auxiliary functions. Using the arrow keys, the
page keys or clicking on the appropriate commands allows you to scroll
through the data; every button and its keyboard shortcut also reaches the
table's focus in reading order. Clicking on Left shifts the data columns
to the left and Right moves them to the right. The time column always
stays fixed. If you do parametric or range calculations, the range
variable is kept in the time column. Home takes you to the top of the
data and End to the last row. The remaining commands are described
separately below; the keyboard shortcut to invoke each is in parentheses.

### (F)ind

This pops up a window and asks for a variable and a value. It then looks through the data until it comes to the closest value to the specified that the variable takes. It only moves down the file so that you can find successive values by starting at the top.

### (G)et

This makes the top row of data the initial conditions for a new run.

### Re(p)lace

This pops up a window asking you for a column to replace. Then it prompts you for a formula. Suppose you want to replace `AUX1` with `x+y-t` where `x,y` are two variables. Then type this in when prompted and the column that held `AUX1` will be replaced by the values in these columns. Any valid XPP function or user function can be used. Two special symbols can also be used when applied to single variables:
- **@VARIABLE**: replaces the column with the numerical derivative of the variable. You cannot use this within a formula, but once the column is replaced, it can be treated as any other column.
- **&VARIABLE**: replaces the column with the numerical integral. Note that successive applications of the derivative and then the integral will result in the original plus a constant.

### (U)nreplace

This undoes the most recent replacement.

### Fir(s)t

This marks the top row in the DB (nothing is shown)as the start or first row for saving or restoring.

### Last (e)

This marks the top row as the last or end row for saving or restoring.

### (T)able

This lets you save data in a tabulated format that can then be used by XPP as a function or inputs. You must use the ` First` and `Last` keys to mark the desired data you want to save. You are then prompted for the column name, the minimum and maximum you want your independent variable to range and the file name. The result is a table file of the format shown in the section on tables.

### (R)estore

replots the data marked by First and Last. The default is the entire data set

### (W)rite: Save data

Saves numbers to a file in the working directory (see [Using the interface](04-using-the-interface.md#saving-pictures-and-files) for how the browser gets it). It asks three things in turn:

1. **What to save**: *The data table* (the rows marked by `First` and `Last`, every column: T, the variables, the auxiliary quantities) or *What the plot shows* (the current plot window's curves, then its frozen curves, as one long table with the columns `curve`, `x`, `y` and, in a 3D plot, `z`: one row per point, the window's curves numbered 1, 2, ... in order and the frozen curves after them).
2. **The format**, one of:
   - **XPP data (.dat)**: XPP's own, what it has always written (and what a batch run writes as `output.dat`): one row per line, the values separated by blanks, 8 significant digits, no names. It is ASCII readable and reflects the current contents of the DB:

     ```
     t0 x1(t0) ... xn(t0)
     t1 x1(t1) ... xn(t1)
     .
     .
     .
     tf x1(tf) ... xn(tf)
     ```

   - **CSV with a header (.csv)**: a first row of the column names (`T,x,y,...`, or `curve,x,y`), then one row per point, each number written with as many digits as it takes to read back exactly the value XPP stored.
   - **Compressed CSV (.csv.gz)**: the same CSV, gzipped (any gzip tool, Python's `gzip` or `pandas.read_csv` opens it).
   - **NumPy arrays (.npz)**: NumPy's own format (`numpy.load`), a zip of one float64 array per column named after it (`T`, `x`, ...); for what the plot shows, one array per curve (`curve1`, `curve2`, ...) of one row per point and the columns x, y (and z).
3. **The file's name**, suggested as `data` or `curves` with the format's extension.

### (L)oad

Loads a data file into the table for graphing: its columns fill T and the columns after it in order (more columns than the table has are left out). The format is the file's extension: `.csv`, `.csv.gz` and `.npz` as Save data writes them (a CSV's header row, when it has one, is skipped; an `.npz` array of two dimensions gives one column per column), anything else is read as XPP's `.dat`. A malformed number or a row with a different number of fields refuses the whole table.

### (A)ddcol

This allows you to add an additional column to the data browser. You are prompted for the name you want to give the column and for the formula. It is computed at once from the stored data, and again every time you run the model afresh, as though it were another auxiliary variable. Unlike a real auxiliary variable, though, it is not one of your model's own quantities: you cannot use its name in another formula (an Addcol's own or one of the model's), and it is lost as soon as you reload the model (revert, or open another one).

### (D)elcol

Delete a column created with `Addcol` by entering its name. Time and model columns are protected. If a plot uses that column, change its axes first. Remaining added columns keep their names, values and formulas and recompute on the next run. Added column names cannot be used in other formulas.
