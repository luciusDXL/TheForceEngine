# Formatting Differences
[Back](local://InfScript)
The way the INF script appears in the editor is a bit different than the way it looks in the raw *.INF files. This document explains the differences and the reasoning behind them. Note this does not change functionality and none of the original data is changed at all - simply the way it is displayed and edited in the editor.

  * In the source data +Y is up in the **scripts** but -Y is up in the **levels** themselves. As a result of floor height of -10 in the level will be shown as 10 in the INF file. This is very confusing, especially to people new to Dark Forces. In the DarkXL 2 level editor, the heights are shown in the same space as the map, in other words a height of -10 in the level is -10 in the INF editor.
  * INF has **messages** and functions. Most messages have targets but not all of them. And both messages and functions can have arguments, can be called from Stops and Triggers and are otherwise identical. So DarkXL 2 just calls all of them functions.
  * The message/function syntax includes the stop number which is not needed since they are assigned to the correct Stops during parsing. Arguments are shown with the function itself in parenthesis () in DarkXL 2, similar to most programming languages.
    
  So instead of showing: **message: 1 complete complete 1**  -> message: stopNum type target argument1
  You will see this in the editor: **Func: complete(1) Target: complete**  -> Func: name(arguments) Target: target
  
  Which is the same format for functions such as Page or Adjoin:
  So instead of showing: **page: 1 m01kyl01.voc**
  You will see in the editor: **Func: page(m01kyl01.voc)**