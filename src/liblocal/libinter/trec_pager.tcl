
#####  Procedures  #####
#----  Clear Mssage  ---##
proc ClearMsg { w_msg } {
  $w_msg configure -text ""
}

#----  Display external message  ----#
proc ExterMsg { w_msg msgtext } {
  set msgmark "!@#$%"
  set msgmark_length [string length $msgmark]

  if { [string first $msgmark $msgtext] == 0 } {
    $w_msg configure -foreground red
    $w_msg configure -text [string range $msgtext [expr $msgmark_length] end]

    update idletasks   ; # Display immediately

    return 1
  } else {
    return 0
  }
} 

#----  Display internal message  ----#
proc InterMsg { w_msg msgtext color } {
  $w_msg configure -text $msgtext -foreground $color
  update idletasks    ; # Display immediately
}

#----  Set mouse ptr busy and disable all input  ----#
proc DisableInput { w  } {
  . config -cursor watch
  grab set $w
}

#----  Set mouse ptr free and enable all input  ----#
proc EnableInput { w } {
  . config -cursor left_ptr
  grab release $w
}

#----  Position of "j" in the title list set by Smart 
proc GetMarkPos { } {
  return 8
}

#----  Minimum count of judge 
proc GetMinJudge { } {
  return 1
}

#----  Listbox with Scrollbars  ----#
proc ScrolledListbox { p args } {
  frame $p
  eval {listbox $p.list \
                -selectmode single \
                -yscrollcommand [list $p.sy set] \
                -xscrollcommand [list $p.sx set]}  $args
  scrollbar $p.sx -orient horizontal -command [list $p.list xview]
  scrollbar $p.sy -orient vertical   -command [list $p.list yview]
  pack $p.sx -side bottom -fill x
  pack $p.sy -side right  -fill y
  pack $p.list -side left -fill both -expand true 
  return $p.list
}

#----  Text with Scrollbars  ----#
proc ScrolledText { p args } {
  frame $p
  eval {text $p.text -yscrollcommand [list $p.sy set]} $args
  scrollbar $p.sy -orient vert -command [list $p.text yview]
  pack $p.sy -side right -fill y
  pack $p.text -side left -fill both -expand true
  return $p.text
}

#----  Clear listbox and set items  ----#
proc ShowList { w_list dlm items listindex } {
  $w_list delete 0 [$w_list index end]

  set s 0
  set lx 0
  set items_end [string length $items] 
  while {$s < $items_end} {
    set e [expr $s+[string first $dlm [string range $items $s $items_end]]]  
    $w_list insert end [string range $items $s [expr $e-1]]
    set s [expr $e+1]
    incr lx 1
  }
  if { $listindex >= 0 } {
    $w_list select set $listindex $listindex   ; # Select the current line
  }
  return 0
}  

#----  Mark a title with judgement  ----#
proc MarkTitle { w_list cur_index mark0 mark1 } {
  set end_index [$w_list index end]
  set mark_pos0 [GetMarkPos]
  set mark_pos1 [expr $mark_pos0+1]  

  set cur_item [lindex [$w_list get $cur_index $cur_index] 0]

  $w_list delete $cur_index $cur_index

  set new_itemA [string range $cur_item 0 [expr $mark_pos0-1]]

  if {$mark0 != ""} {
    set new_item0 $mark0
  } else {
    set new_item0 [string range $cur_item $mark_pos0 $mark_pos0]
  }  
  if {$mark1 != ""} {
    set new_item1 $mark1
  } else {
    set new_item1 [string range $cur_item $mark_pos1 $mark_pos1]
  }
  set new_itemB [string range $cur_item [expr $mark_pos1+1] end]

  $w_list insert $cur_index \
        [format "%s%s%s%s" $new_itemA $new_item0 $new_item1 $new_itemB] 
  $w_list select clear 0 $end_index
  $w_list select set $cur_index $cur_index

  return $cur_index
}


#----  Set highlight tag  ----# 
proc ShowDoc_Highlight { w_text doc } {
#  set high_start [format "%s%s7m" "\x1b" "\x5b"]
#  set high_end   [format "%s%sm"  "\x1b" "\x5b"] 
  set high_start [qqqGetHighlightStr 0]
  set high_end   [qqqGetHighlightStr 1]

  set high_start_len [string length $high_start]
  set high_end_len [string length $high_end]

  $w_text tag configure hlight -foreground blue ; # Highlight attribute 
 
  #----  
  set p 0
  set doc_end [string length $doc]

  while { $p < $doc_end } {  
    set n0 $p    ; # Start of normal
    set s [string first $high_start [string range $doc $p end]]
    if { $s < 0 } {
      set s $doc_end
    } else {
      set s [expr $s+$p]
    }
    set n1 $s    ; # End of normal
   
    $w_text insert end [string range $doc $n0 [expr $n1-1]]
    set index0 [$w_text index end]

    set s [expr $s+$high_start_len]
    set h0 $s    ; # Start of highlight
 
    if { $s >= $doc_end } break

    set p $s
    set s [string first $high_end [string range $doc $p end]]
    if { $s < 0 } {
      set s $doc_end
    } else {
      set s [expr $s+$p]
    }

    set h1 $s    ; # End of highlight

    $w_text insert end [string range $doc $h0 [expr $h1-1]] hlight
   
    set p [expr $s+$high_end_len]
  }
}
 
#----  Clear doc widget  ----#
proc ClearDoc { w_text } {
 $w_text delete 1.0 [$w_text index end]
}
 
#----  Display contents of a document  ----#  
proc ShowDoc { w_text w_list } {
  global w_txtmsg w_msg 
  InterMsg $w_txtmsg "Updating doc widget. Be patient..." blue

  ClearDoc $w_text
  set current_doc [lindex [$w_list curselection] 0]

  if { $current_doc == "" } {
    InterMsg $w_msg "No document is selected!" red
    ClearMsg $w_txtmsg
    return -1
  }

  set result [qqqShowDoc $current_doc]
  if { [ExterMsg $w_msg $result] != 1 } {
    ShowDoc_Highlight $w_text $result

#    MarkTitle $w_list $current_doc "*" ""
    ClearMsg $w_txtmsg
    return $current_doc
  } else {
    ClearMsg $w_txtmsg
    return -1
  }
} 

#----  Save judgement in C array  ----# 
proc SaveJudgeAll { w_list } { 
  set lastindex [$w_list index end]
  set mark_pos [GetMarkPos]  
  set judged_num 0
  for {set lx 0} { $lx < $lastindex} {incr lx 1} {
    set item [lindex [$w_list get $lx $lx] 0]
    if { [string range $item $mark_pos $mark_pos] != " " } {
      incr judged_num 1
    }
    qqqSaveJudge $lx [string range $item $mark_pos [expr $mark_pos+1]]
  }
  return $judged_num
}


#----  Check the status of a child process  ----#
proc ChildStateDone {} {
  if { [string first "DONE" [qqqCheckChildState]] == 0 } {
    return 1
  } else {
    return 0
  }
}

#----  Check the status of a child process  ----#
proc ChildStateRunning {} {
  if { [string first "RUNNING" [qqqCheckChildState]] == 0 } {
    return 1
  } else {
    return 0
  }
}


#---- 
proc ShowResult { result w_list w_text current_doc } {
  global w_msg
  if { [ExterMsg $w_msg $result] != 1 } {  ; # No error message 
    ShowList  $w_list  "\n"  $result $current_doc
    update idletasks                    ; # Display list immediately
    return [ShowDoc $w_text $w_list]   
  } else {                              ; # Some error 
    ShowList  $w_list  "\n"  "" -1      ; # Clear list
    ClearDoc   $w_text                  ; # Clear doc          
    return -1
  } 
  #---  Return shown_doc  ---#
}

#----
proc ShowVector { w_vec } {
  $w_vec delete 1.0 [$w_vec index end]
  $w_vec insert end [qqqGetVector]
}

#----
proc RunQuery { query w_vec w_list w_text current_doc } {
  global w_msg
  if { [ChildStateRunning] == 1 } {
    InterMsg $w_msg "Child process is still running.  Try later." red
    return $current_doc
  }
  if { [string length $query] == 0 } {
    InterMsg $w_msg "No query!" red
    return $current_doc
  }

  set result [qqqRunQuery $query] 

  ShowVector $w_vec

  return [ShowResult $result $w_list $w_text 0]

  #---  Return shown_doc  ---#
}


#----  Clear judgement from the list  ----# 
proc ClearJudgeAll { w_list } { 
  set lastindex [$w_list index end]
  set cur_index [lindex [$w_list curselection] 0]
  set mark_pos [GetMarkPos]  

  for {set lx 0} { $lx < $lastindex} {incr lx 1} {
    set item [lindex [$w_list get $lx $lx] 0]
    if { [string range $item $mark_pos $mark_pos] == "j" } {
      MarkTitle $w_list $lx " " ""
      $w_list select clear $lx $lx
    }
  }
  $w_list select set $cur_index $cur_index  
}

#----
proc RunFeedback { w_list } {
  global w_msg
  if { [ChildStateRunning] == 1} {
    InterMsg $w_msg "Child process is still running. Try later." red
    return
  } 

  if { [ExterMsg $w_msg [qqqRunFeedback]] != 1} {
    InterMsg $w_msg "Child process started..." blue
  }
}  

#----
proc ModifyQuery { query w_list } {
  global w_msg
  if { [ChildStateRunning] == 1} {
    InterMsg $w_msg "Child process is still running. Try later." red
    return
  } 
  if { [string length $query] == 0 } {
    InterMsg $w_msg "No query!" red
    return
  }

  if { [ExterMsg $w_msg [qqqModifyQuery $query]] != 1} {
    InterMsg $w_msg "Child process started..." blue
  }
}  

#----
proc ModifyVector { vector w_list } {
  global w_msg
  if { [ChildStateRunning] == 1} {
    InterMsg $w_msg "Child process is still running. Try later." red
    return
  } 

  if { [ExterMsg $w_msg [qqqModifyVector $vector]] != 1} {
    InterMsg $w_msg "Child process started..." blue
  }
}  

#----  Merge the reulst of child process  ----#
proc MergeChildResult { w_list w_text current_doc } {
  global w_msg
  if { [ChildStateDone] != 1 } {
    $w_list select clear $current_doc $current_doc
    set current_doc [expr $current_doc + 1]
    $w_list select set $current_doc $current_doc   ; # Select the current line
    return [ShowDoc $w_text $w_list]
  }

  ClearMsg $w_msg
  InterMsg $w_msg "Child run done.  Showing new docs." blue
  DisableInput $w_msg

  set result [qqqMergeNewList]
  set cur_doc [ShowResult $result $w_list $w_text [expr [qqqGetCurrDoc]]]
  EnableInput $w_msg     
  return $cur_doc

  #---  Return shown_doc  ---#
}

#----  AutoFeedback
proc AutoFeedback { w_list w_j_count w_p_count } {
  #---  Initialize  ---#
  set lastindex [$w_list index end]
  set j_count 0 
  set p_count 0 
  set mark_pos0 [GetMarkPos]
  set mark_pos1 [expr $mark_pos0+1]

  #---  Get minimum # of judge and positive judge  ---#
  set judge_count [$w_j_count get]
  set positive_count [$w_p_count get]

  #---  Count judge  ---#
  for {set lx 0} { $lx < $lastindex} {incr lx 1} {
    set item [lindex [$w_list get $lx $lx] 0]
    #---  count j  ---#
    if { [string range $item $mark_pos0 $mark_pos0] == "j" } {
      incr j_count 1
      #---  count relevant  ---#
      if { [string range $item $mark_pos1 $mark_pos1] != "N" } {
        incr p_count 1 
      }
    } 
  }

puts "j_count"; puts $j_count
puts "p_count"; puts $p_count
  
  if { ($j_count >= $judge_count) && ($p_count >= $positive_count) } {
    RunFeedback $w_list    
  }
}

#----  ResetElapsedTime  
proc ResetElapsedTime { w } {
  qqqResetElapsedTime
  $w configure -text         "        Elapsed time:   0.0"
  update idletasks
}

#----  Update elapsed time 
proc UpdateElapsedTime { w } {
  $w configure -text [format "        Elapsed time: %s" [qqqGetElapsedTime]]
  update idletasks
}    



################################
#      Initialize Widgets      #
###############################
wm title . "ctrec_pager" 

#---  frame for query, titles, vector 
frame .p1   
frame .p2  

#---- message for updating doc contents - label 
label .p2.tm -foreground blue -width 80 -justify left

#---- message - label
label .p1.m -foreground red -width 60 -justify left

#---- query - entry
frame .p1.q 
set w_q .p1.q
entry $w_q.entry -width 64
pack $w_q.entry

#---- run buttons 
frame .p1.r
set w_r .p1.r
button $w_r.new_button -text "New query"
button $w_r.run_button -text "Submit query"
button $w_r.fdbk_button -text "Rel fdbk"
button $w_r.modq_button -text "Modify query"
button $w_r.modv_button -text "Modify vector"
pack $w_r.new_button $w_r.run_button $w_r.fdbk_button \
    $w_r.modq_button $w_r.modv_button -side left -fill x

#----  query vector - text 
frame .p1.v
set w_v .p1.v
ScrolledText $w_v.vector -width 60 -height 20
pack $w_v.vector -side left -fill both -expand true

#---- doc list - listbox
frame .p1.l
set w_l .p1.l
ScrolledListbox $w_l.doclist  -width 60 -height 20
pack $w_l.doclist -side left -fill both -expand true

#----  judge buttons 
frame .p1.j
set w_j .p1.j
button $w_j.rel_button  -text "Rel"
button $w_j.nrel_button -text "Non rel "
button $w_j.prel_button -text "Poss rel "
button $w_j.reset_button -text "Reset jdgmnt"
pack $w_j.rel_button $w_j.nrel_button $w_j.prel_button $w_j.reset_button -side left -fill x

#---- text - text
frame .p2.t -width 80
set w_t .p2.t
ScrolledText $w_t.doc -width 80 -height 60
pack $w_t.doc -side left -fill both -expand true

#---- minimum # of judge for automatical relevance feedback 
frame .p1.o 
set w_o .p1.o 
label $w_o.j_label -text "Min judge"
entry $w_o.j_count -width 4; $w_o.j_count insert 0 "5"
label $w_o.p_label -text "Min positive"
entry $w_o.p_count -width 4; $w_o.p_count insert 0 "1"
label $w_o.elapsed
pack $w_o.j_label $w_o.j_count $w_o.p_label $w_o.p_count $w_o.elapsed \
     -side left

#---- quit button
frame .p1.qf
button .p1.qf.qbut -text "Quit"
pack .p1.qf .p1.qf.qbut -side bottom

#----
#pack .p1.m .p1.c .p1.l .p1.j .p1.q .p1.r .p1.v .p1.b -side top -fill both
pack .p1.m -side top -fill both -padx 2m
pack $w_l -side top -fill both -padx 2m -pady 2m
pack $w_j -side top -fill both -padx 2m
pack $w_q -side top -fill both -padx 2m -pady 2m
pack $w_r -side top -fill both -padx 2m 
pack $w_v -side top -fill both -padx 2m -pady 2m
pack $w_o -side top -fill both -padx 2m

pack .p2.t .p2.tm  -side top -fill both -padx 2m -pady 2m

pack .p1 .p2 -side left -fill both
#pack .m .c .q .r .v .b .l .j .t .tm -side top -padx 2m -pady 1m -fill x

focus $w_q.entry

###########################
set shown_doc -1

set w_msg      .p1.m
set w_txtmsg   .p2.tm
set w_query    $w_q.entry
set w_vector   $w_v.vector.text
set w_list     $w_l.doclist.list
set w_doc      $w_t.doc.text
set b_judge    $w_j
set b_query    $w_r 
set w_elapsed  $w_o.elapsed
set w_j_count  .p1.o.j_count
set w_p_count  .p1.o.p_count

#ResetElapsedTime $w_elapsed

###########################  Bind
bind all <Destroy> { puts "Destroy!"; qqqCloseAll }

#-----------  "New query" button
bind $b_query.new_button <ButtonPress> {
  DisableInput $w_msg
  while { [ChildStateRunning] == 1 } {
      InterMsg $w_msg "Waiting for old child process to finish..." blue
      sleep 5
  }
  ClearMsg $w_msg
  ResetElapsedTime $w_elapsed
  EnableInput $w_msg
}

#-----------  "Run query" button
bind $b_query.run_button <ButtonPress> {
  DisableInput $w_msg
  ClearMsg $w_msg

  set shown_doc [RunQuery [$w_query get] $w_vector $w_list \
                           $w_doc $shown_doc ]
  EnableInput $w_msg
  UpdateElapsedTime $w_elapsed
}

#-----------  "Run feedback" button
bind $b_query.fdbk_button <ButtonPress> {
  DisableInput $w_msg
  ClearMsg $w_msg
  RunFeedback $w_list
  EnableInput $w_msg
}

#-----------  "Modify query" button
bind $b_query.modq_button <ButtonPress> {
  DisableInput $w_msg
  ClearMsg $w_msg
  ModifyQuery [$w_query get] $w_list
  EnableInput $w_msg
} 

#-----------  "Modify vector" button
bind $b_query.modv_button <ButtonPress> {
  DisableInput $w_msg
  ClearMsg $w_msg
  ModifyVector [$w_vector get 1.0 end] $w_list
  EnableInput $w_msg
} 

#-----------  Change title selection
#bind .l.doclist.list <ButtonPress> {
#  MarkTitle .l.doclist.list $shown_doc "j" ""
#}

#-----------  Select title
bind $w_list <ButtonRelease> {
  DisableInput $w_msg
  ClearMsg $w_msg

  set shown_doc [ShowDoc $w_doc $w_list]
  EnableInput $w_msg

  set shown_doc [MergeChildResult  $w_list $w_doc $shown_doc]
  UpdateElapsedTime $w_elapsed
}

#-----------  "Relevant" button 
bind $b_judge.rel_button <ButtonPress> {
  ClearMsg $w_msg
  MarkTitle $w_list $shown_doc "j" "Y"
  qqqProcessJudge $shown_doc "Y"
# show next doc
  set shown_doc [MergeChildResult  $w_list $w_doc $shown_doc]
}

#-----------  "Non-relevant" button 
bind $b_judge.nrel_button <ButtonPress> {
  ClearMsg $w_msg
  MarkTitle $w_list $shown_doc "j" "N"
  qqqProcessJudge $shown_doc "N"
# show next doc
  set shown_doc [MergeChildResult  $w_list $w_doc $shown_doc]
}

#-----------  "Possibly-relevant" button 
bind $b_judge.prel_button <ButtonPress> {
  ClearMsg $w_msg
  MarkTitle $w_list $shown_doc "j" "P"
  qqqProcessJudge $shown_doc "P"
# show next doc
  set shown_doc [MergeChildResult  $w_list $w_doc $shown_doc]
}

#-----------  "Reset judge" button 
bind $b_judge.reset_button <ButtonPress> {
  ClearMsg $w_msg
  MarkTitle $w_list $shown_doc " " " "
  set shown_doc [MergeChildResult  $w_list $w_doc $shown_doc]
}

#-----------  Update elapsed time when something is clicked
bind all <ButtonPress> {
  UpdateElapsedTime $w_elapsed
}

#-----------  "Quit" button 
bind .p1.qf.qbut <ButtonPress> {
  qqqCloseAll
  exit
}
