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

### (W)rite

prompts you for a filename and writes the marked data to a file, in the working directory (see [Using the interface](04-using-the-interface.md#saving-pictures-and-files) for how the browser gets it). The file is ASCII readable and reflects the current contents of the DB:

```
t0 x1(t0) ... xn(t0)
t1 x1(t1) ... xn(t1)
.
.
.
tf x1(tf) ... xn(tf)
```

### (L)oad

Will load in as much of a similarly formatted data file as possible for graphing.

### (A)ddcol

This allows you to add an additional column to the data browser. You are prompted for the name you want to give the column and for the formula. It is thus, like an auxiliary variable with a name. Thereafter, it will be computed along with any other quantities that you have defined. It is as though you had included another auxiliary variable in your original file.

### (D)elcol

This lets you delete a column. You can only delete columns which you have created with the `Addcol` command.
- You can delete a column that is itself referred to by a different column; this will result in wrong answers in the column. Thus, do not delete columns whose contents are used by other columns. Also, if you delete a column, its name is still known by the internal system but it has no real value. For these reasons, you should delete columns with caution. If the purpose of deleting them is to change the formula, use the right-hand-side editor, (File-Edit) instead.
