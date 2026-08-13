BEGIN {
   timelimit = 14400
   timelimit2 = 86400
   if (gurobi_timelimit == "")
      gurobi_timelimit = 14400
   timeshift = 1
   nodeshift = 100

   if (table == "")
      table = "all"

   if (table !~ /^(1|2|3|4|5|6|all|detail)$/) {
      printf "ERROR: unsupported table '%s'; use 1, 2, 3, 4, 5, 6, all, or detail\n", table > "/dev/stderr"
      fatal = 1
      exit 2
   }

   expand_dir_arguments()

   if (ResultSCIP != "")
      add_files_from_dir(ResultSCIP)

   if (ResultGurobi != "")
      add_files_from_dir(ResultGurobi)

   if (root != "")
      add_files_from_dir(root)

   if (ARGC <= 1 && root == "" && ResultSCIP == "" && ResultGurobi == "") {
      print "usage: awk -f scripts/CCP.awk [-v ResultSCIP=results-scip] [-v ResultGurobi=results-gurobi] [-v table=1|2|3|4|5|6|all|detail]" > "/dev/stderr"
      fatal = 1
      exit 2
   }
}

function shell_quote(s, t) {
   t = s
   gsub(/["\\$`]/, "\\\\&", t)
   return "\"" t "\""
}

function add_files_from_dir(dir, cmd, f) {
   cmd = "find " shell_quote(dir) " -type f -name '*.out' | sort"
   while ((cmd | getline f) > 0)
      ARGV[ARGC++] = f
   close(cmd)
}

function expand_dir_arguments(   i, n, arg, cmd) {
   n = ARGC
   for (i = 1; i < n; ++i) {
      arg = ARGV[i]
      if (arg == "")
         continue
      cmd = "test -d " shell_quote(arg)
      if (system(cmd) == 0) {
         add_files_from_dir(arg)
         ARGV[i] = ""
      }
   }
}

function reset_metrics() {
   tmpobjval = -1
   tmpbestobjval = -1
   tmptime = -1
   tmpgap = -1
   tmpnode = -1
   tmpDom = 0
   tmptureDom = 0
   tmpallDom = 0
   tmpDomReds = 0
   tmpFixedTotal = 0
   tmpFixedSeen = 0
   Tbound = 0
   CutOffK = 0
   tmpCutoffs = 0
   exactcutoff = 0
   somechancetime = 0
   somepropgraphtime = 0
   tmpscennum = 0
   flag = 0
}

function basename(path, b) {
   b = path
   sub(/^.*\//, "", b)
   return b
}

function last_index(s, ch, i) {
   for (i = length(s); i >= 1; --i)
      if (substr(s, i, 1) == ch)
         return i
   return 0
}

function prob_from_lc(p) {
   if (p == "ccrp")
      return "CCRP"
   if (p == "ccmpp")
      return "CCMPP"
   if (p == "ccls")
      return "CCLS"
   return ""
}

function suffix_from_lc(p) {
   if (p == "ccrp")
      return "rpp"
   if (p == "ccmpp")
      return "mpp"
   if (p == "ccls")
      return "lsp"
   return p
}

function first_token(s, a) {
   split(s, a, "-")
   return a[1]
}

function normalize_setting(s) {
   if (s == "BASE")
      return "BnC-MIX"
   if (s == "BASE+DI")
      return "BnC-MIX-DI"
   if (s == "BASE+sDI")
      return "BnC-MIX-sDI"
   if (s == "BASE+DB")
      return "DB"
   if (s == "BASE+DB+OPF")
      return "DB-OPF"
   if (s == "BASE+DB+EOPF")
      return "DB-EOPF"
   return s
}

function parse_result_path(path, bn, stem, p, rest, d, prob_lc, suffix) {
   current_prob = ""
   current_instance = ""
   current_eps = ""
   current_setting = ""
   current_method = ""
   current_limit = "14400"
   current_name = ""
   current_m = ""
   skipfile = 0

   bn = basename(path)
   stem = bn
   sub(/\.out$/, "", stem)

   if ((p = index(stem, ".ccrp-")) > 0) {
      prob_lc = "ccrp"
      current_instance = substr(stem, 1, p - 1)
      rest = substr(stem, p + length(".ccrp-"))
   }
   else if ((p = index(stem, ".ccmpp-")) > 0) {
      prob_lc = "ccmpp"
      current_instance = substr(stem, 1, p - 1)
      rest = substr(stem, p + length(".ccmpp-"))
   }
   else if ((p = index(stem, ".ccls-")) > 0) {
      prob_lc = "ccls"
      current_instance = substr(stem, 1, p - 1)
      rest = substr(stem, p + length(".ccls-"))
   }
   else {
      skipfile = 1
      return
   }

   d = last_index(rest, "-")
   if (d == 0) {
      skipfile = 1
      return
   }

   current_setting = normalize_setting(substr(rest, 1, d - 1))
   current_eps = substr(rest, d + 1)
   current_prob = prob_from_lc(prob_lc)
   suffix = suffix_from_lc(prob_lc)
   current_m = first_token(current_instance)

   if (path ~ /\/(DB-OPF|BASE\+DB\+OPF)\/86400\//)
      current_limit = "86400"
   else if (path ~ /\/(DB-OPF|BASE\+DB\+OPF)\/14400\//)
      current_limit = "14400"

   if (current_setting == "DB-OPF")
      current_method = current_setting "@" current_limit
   else
      current_method = current_setting

   current_name = current_instance "-" current_eps "-" suffix
}

function parse_data_path(path, bn, p, prob_lc, suffix) {
   bn = basename(path)
   if ((p = index(bn, ".ccrp")) > 0) {
      prob_lc = "ccrp"
      if (current_instance == "")
         current_instance = substr(bn, 1, p - 1)
   }
   else if ((p = index(bn, ".ccmpp")) > 0) {
      prob_lc = "ccmpp"
      if (current_instance == "")
         current_instance = substr(bn, 1, p - 1)
   }
   else if ((p = index(bn, ".ccls")) > 0) {
      prob_lc = "ccls"
      if (current_instance == "")
         current_instance = substr(bn, 1, p - 1)
   }
   else
      return

   if (current_prob == "")
      current_prob = prob_from_lc(prob_lc)

   if (current_name == "" && current_eps != "") {
      suffix = suffix_from_lc(prob_lc)
      current_m = first_token(current_instance)
      current_name = current_instance "-" current_eps "-" suffix
   }
}

function isnum(x) {
   return x ~ /^[-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?$/
}

function lastnum(   i) {
   for (i = NF; i >= 1; --i)
      if (isnum($i))
         return $i + 0
   return 0
}

function min(a, b) {
   return (a < b) ? a : b
}

function gmean_update(cur, data, shift, num) {
   return (data + shift) ^ (1 / (num + 1)) * cur ^ (num / (num + 1))
}

function add_problem(name, prob, m) {
   if (!(name in probseen)) {
      probseen[name] = 1
      problist[++probnum] = name
   }
   probtype[name] = prob
   mvalue[name] = m + 0
}

function commit_record(   key, scendenom, st) {
   if (skipfile || current_name == "" || current_method == "")
      return

   add_problem(current_name, current_prob, current_m)
   key = current_name SUBSEP current_method
   probfile[key] = 1
   delete badfile[key]

   pb[key] = tmpobjval
   db[key] = tmpbestobjval
   nodes[key] = (tmpnode < 0) ? 0 : tmpnode
   timev[key] = (tmptime < 0) ? method_limit(current_method) : tmptime
   gaps[key] = tmpgap
   scennum[key] = tmpscennum
   dr[key] = tmpDom * 100

   scendenom = tmpscennum * (tmpscennum - 1) / 2
   if (scendenom > 0)
      truedr[key] = tmptureDom / scendenom * 100
   else
      truedr[key] = 0

   if (tmpFixedSeen)
      domreds[key] = tmpFixedTotal
   else
      domreds[key] = tmpDomReds - Tbound
   detaildomreds[key] = tmpDomReds
   graphTime[key] = somepropgraphtime
   alldom[key] = tmpallDom
   turedom[key] = tmptureDom
   cutoffs[key] = tmpCutoffs + CutOffK
   cutoffsea[key] = exactcutoff
   chancetime[key] = somechancetime + somepropgraphtime

   st = (flag == 0) ? "stopped" : "ok"
   status[key] = st
   current_committed = 1
}

function commit_incomplete_record(   key) {
   if (skipfile || current_name == "" || current_method == "")
      return

   add_problem(current_name, current_prob, current_m)
   key = current_name SUBSEP current_method
   if (!(key in probfile))
      badfile[key] = 1
}

function finish_uncommitted_file() {
   if (current_file_started && !current_committed)
      commit_incomplete_record()
   current_file_started = 0
}

function method_limit(method) {
   if (method == "DB-EOPF" || method ~ /@86400$/)
      return timelimit2
   return timelimit
}

function g_parse_result_path(path, bn, stem, p, rest, marker, prob_lc, suffix) {
   g_current_prob = ""
   g_current_instance = ""
   g_current_eps = ""
   g_current_method = ""
   g_current_name = ""
   g_current_m = ""
   g_skipfile = 0

   bn = basename(path)
   stem = bn
   sub(/\.out$/, "", stem)

   if ((p = index(stem, ".ccrp-")) > 0)
      prob_lc = "ccrp"
   else if ((p = index(stem, ".ccmpp-")) > 0)
      prob_lc = "ccmpp"
   else if ((p = index(stem, ".ccls-")) > 0)
      prob_lc = "ccls"
   else {
      g_skipfile = 1
      return
   }

   g_current_instance = substr(stem, 1, p - 1)
   rest = substr(stem, p + length("." prob_lc "-"))

   if (index(rest, "BASE+sDI-MIXROOT-LAZY1-") == 1) {
      g_current_method = "BASE+sDI"
      marker = "BASE+sDI-MIXROOT-LAZY1-"
   }
   else if (index(rest, "BASE+DI-MIXROOT-LAZY1-") == 1) {
      g_current_method = "BASE+DI"
      marker = "BASE+DI-MIXROOT-LAZY1-"
   }
   else if (index(rest, "BASE-MIX-") == 1) {
      g_current_method = "BASE-MIX"
      marker = "BASE-MIX-"
   }
   else if (index(rest, "BASE-NOMIX-") == 1) {
      g_current_method = "BASE-MIX"
      marker = "BASE-NOMIX-"
   }
   else if (index(rest, "BASE+sDI-") == 1) {
      g_current_method = "BASE+sDI"
      marker = g_current_method "-"
   }
   else if (index(rest, "BASE+DI-") == 1) {
      g_current_method = "BASE+DI"
      marker = g_current_method "-"
   }
   else if (index(rest, "BASE-MIXROOT-") == 1) {
      g_current_method = "BASE"
      marker = "BASE-MIXROOT-"
   }
   else if (index(rest, "BASE-") == 1) {
      g_current_method = "BASE"
      marker = g_current_method "-"
   }
   else {
      g_skipfile = 1
      return
   }

   g_current_eps = substr(rest, length(marker) + 1)
   g_current_prob = prob_from_lc(prob_lc)
   g_current_m = first_token(g_current_instance) + 0
   suffix = suffix_from_lc(prob_lc)
   g_current_name = g_current_instance "-" g_current_eps "-" suffix
}

function g_reset_record() {
   g_tmptime = -1
   g_tmpnode = -1
   g_tmpobjective = 0
   g_tmpobjective_seen = 0
   g_tmpscennum = 0
   g_tmpdomratio = 0
   g_tmpnondom = 0
   g_tmptermination = ""
   g_native_finished = 0
   g_native_solved = 0
   g_seen_log = 0
   g_current_committed = 0
}

function g_add_problem(name, prob, m, eps) {
   if (!(name in g_probseen)) {
      g_probseen[name] = 1
      g_problist[++g_probnum] = name
   }
   g_probtype[name] = prob
   g_mvalue[name] = m
   g_epsvalue[name] = eps + 0
}

function g_is_solved_status(st) {
   return st == "OPTIMAL"
}

function g_commit_record(   key, denom) {
   if (g_skipfile || g_current_name == "" || g_current_method == "")
      return
   if (!g_seen_log)
      return
   if (g_tmptermination == "" && !g_native_finished)
      return

   g_add_problem(g_current_name, g_current_prob, g_current_m, g_current_eps)
   key = g_current_name SUBSEP g_current_method
   g_probfile[key] = 1
   g_status[key] = (g_is_solved_status(g_tmptermination) || g_native_solved) ? "ok" : "stopped"
   g_timev[key] = (g_tmptime < 0) ? gurobi_timelimit : g_tmptime
   g_nodes[key] = (g_tmpnode < 0) ? 0 : g_tmpnode
   if (g_tmpobjective_seen) {
      g_objective[key] = g_tmpobjective
      g_objective_seen[key] = 1
   }
   g_dr[key] = g_tmpdomratio * 100

   denom = g_tmpscennum * (g_tmpscennum - 1) / 2
   g_tdr[key] = (denom > 0) ? g_tmpnondom / denom * 100 : 0
   g_current_committed = 1
}

function g_finish_file() {
   if (!g_current_file_started)
      return
   g_commit_record()
   if (!g_skipfile && g_seen_log && g_current_name != "" && g_current_method != "" && !g_current_committed)
      ++g_incomplete_files
   g_current_file_started = 0
}

FNR == 1 {
   finish_uncommitted_file()
   g_finish_file()
   reset_metrics()
   g_reset_record()
   parse_result_path(FILENAME)
   g_parse_result_path(FILENAME)
   current_file_started = 1
   current_committed = 0
   g_current_file_started = 1
}

/read problem / {
   reset_metrics()
}

/File name:/ {
   parse_data_path($3)
}

/^NumScenario:/ {
   tmpscennum = $2 + 0
}

/^[[:space:]]*Primal Bound[[:space:]]*:/ {
   tmpobjval = $4 + 0
}

/^[[:space:]]*Dual Bound[[:space:]]*:/ {
   tmpbestobjval = $4 + 0
}

/Solving Time \(sec\)/ {
   tmptime = $5 + 0
}

/^[[:space:]]*solving[[:space:]]*:/ {
   tmptime = $3 + 0
}

/^[[:space:]]*Gap[[:space:]]*:/ {
   tmpgap = $3 + 0
}

/Solving Nodes/ {
   tmpnode = $4 + 0
}

/nodes \(total\)/ {
   tmpnode = $4 + 0
}

/DominantRatio:/ || /DominanceRatio:/ {
   tmpDom = $2 + 0
}

/numNonRePreCons:/ || /NumNonReDomIneqs:/ {
   tmptureDom = $2 + 0
}

/numPreCons:/ || /NumDomIneqs:/ {
   tmpallDom = $2 + 0
}

/^[[:space:]]*chancecons[[:space:]]*:/ {
   if (NF == 17)
      tmpDomReds = $13 + 0
   if (NF == 12)
      somechancetime = $6 + $12
}

/Tighten times of node lower bound for variable v/ {
   Tbound = $11 + 0
}

/Total Fixed One Variables of Propagator/ || /Total number of variables fixed to 1 by the propagator/ {
   tmpFixedTotal += lastnum()
   tmpFixedSeen = 1
}

/Total Fixed Zero Variables of Propagator/ || /Total number of variables fixed to 0 by the propagator/ {
   tmpFixedTotal += lastnum()
   tmpFixedSeen = 1
}

/Total Fixed One Variables in exact algorithm/ || /Total number of variables fixed to 1 in the exact algorithm/ {
   tmpFixedTotal += lastnum()
   tmpFixedSeen = 1
}

/Total Fixed Zero Variables in exact algorithm/ || /Total number of variables fixed to 0 in the exact algorithm/ {
   tmpFixedTotal += lastnum()
   tmpFixedSeen = 1
}

/CutOff Times of KnaCons/ || /Number of cutoffs of KnaCons/ {
   CutOffK = lastnum()
}

/CutOff Times in approximation algorithm/ || /Number of cutoffs in the approximation algorithm/ {
   tmpCutoffs = lastnum()
}

/CutOff Times in exact algorithm/ || /Number of cutoffs in the exact algorithm/ {
   exactcutoff = lastnum()
}

/Duration:/ {
   somepropgraphtime = $2 + 0
}

/SCIP Status[[:space:]]*: problem is solved/ {
   flag = 1
}

/PS:/ {
   commit_record()
}

/^NumScenario:[[:space:]]*/ {
   if (!g_skipfile)
      g_tmpscennum = $2 + 0
}

/^NumNonReDomIneqs:[[:space:]]*/ {
   if (!g_skipfile)
      g_tmpnondom = $2 + 0
}

/^DominanceRatio:[[:space:]]*/ {
   if (!g_skipfile)
      g_tmpdomratio = $2 + 0
}

/^TerminationStatus:[[:space:]]*/ {
   if (!g_skipfile) {
      g_seen_log = 1
      g_tmptermination = $2
      g_native_finished = 1
   }
}

/^SolvingTime:[[:space:]]*/ {
   if (!g_skipfile) {
      g_seen_log = 1
      g_tmptime = $2 + 0
   }
}

/^NodeCount:[[:space:]]*/ {
   if (!g_skipfile) {
      g_seen_log = 1
      g_tmpnode = $2 + 0
   }
}

/^ObjectiveValue:[[:space:]]*/ {
   if (!g_skipfile) {
      g_seen_log = 1
      g_tmpobjective = $2 + 0
      g_tmpobjective_seen = 1
   }
}

/Gurobi Optimizer version/ {
   if (!g_skipfile)
      g_seen_log = 1
}

/^Best objective[[:space:]]+/ {
   if (!g_skipfile) {
      g_seen_log = 1
      g_tmpobjective = $3 + 0
      g_tmpobjective_seen = 1
   }
}

/Optimal solution found/ {
   if (!g_skipfile) {
      g_seen_log = 1
      g_native_finished = 1
      g_native_solved = 1
   }
}

/Time limit reached/ {
   if (!g_skipfile) {
      g_seen_log = 1
      g_native_finished = 1
   }
}

/^Explored[[:space:]]+[0-9]+[[:space:]]+nodes/ {
   if (!g_skipfile) {
      g_seen_log = 1
      g_tmpnode = $2 + 0
      for (g_i = 1; g_i < NF; ++g_i) {
         if ($g_i == "in") {
            g_tmptime = $(g_i + 1) + 0
            break
         }
      }
   }
}

function eligible(prob, probfilter, family) {
   if (probfilter != "All" && probtype[prob] != probfilter)
      return 0
   if (family == "main" && probtype[prob] == "CCMPP") {
      if (mvalue[prob] != 10 && mvalue[prob] != 20 && mvalue[prob] != 30)
         return 0
   }
   return 1
}

function aggregate(method_string, probfilter, threshold, require_all, family,   n, i, j, prob, method, key, present, atleast, nodehit, allsolved, data, node_data, f_data, c_data) {
   n = split(method_string, rowmethod, " ")
   rown = 0
   for (j = 1; j <= n; ++j) {
      rowsolved[j] = 0
      rowtime[j] = 1
      rownodes[j] = 1
      rowpt[j] = 1
      rowf[j] = 1
      rowpn[j] = 1
      rowdr[j] = 1
      rowtdr[j] = 1
   }

   for (i = 1; i <= probnum; ++i) {
      prob = problist[i]
      if (!eligible(prob, probfilter, family))
         continue

      present = 1
      atleast = 0
      nodehit = 0
      allsolved = 1

      for (j = 1; j <= n; ++j) {
         method = rowmethod[j]
         key = prob SUBSEP method
         if (probfile[key] != 1) {
            present = 0
            break
         }
         if (status[key] == "ok")
            ++atleast
         else
            allsolved = 0
         if (nodes[key] >= threshold)
            ++nodehit
      }

      if (!present)
         continue
      if (require_all && !allsolved)
         continue
      if (!require_all && atleast == 0)
         continue
      if (nodehit == 0)
         continue

      for (j = 1; j <= n; ++j) {
         method = rowmethod[j]
         key = prob SUBSEP method
         if (status[key] == "ok") {
            ++rowsolved[j]
            data = timev[key]
         }
         else
            data = method_limit(method)

         node_data = nodes[key]
         f_data = (node_data > 0) ? domreds[key] / node_data : 0
         c_data = (method == "DB-EOPF") ? cutoffsea[key] : cutoffs[key]

         rowtime[j] = gmean_update(rowtime[j], data, timeshift, rown)
         rownodes[j] = gmean_update(rownodes[j], node_data, nodeshift, rown)
         rowpt[j] = gmean_update(rowpt[j], chancetime[key], timeshift, rown)
         rowf[j] = gmean_update(rowf[j], f_data, nodeshift, rown)
         rowpn[j] = gmean_update(rowpn[j], c_data, nodeshift, rown)
         rowdr[j] = gmean_update(rowdr[j], dr[key], nodeshift, rown)
         rowtdr[j] = gmean_update(rowtdr[j], truedr[key], nodeshift, rown)
      }
      ++rown
   }

   for (j = 1; j <= n; ++j) {
      if (rown == 0) {
         outtime[j] = 0
         outnodes[j] = 0
         outpt[j] = 0
         outf[j] = 0
         outpn[j] = 0
         outdr[j] = 0
         outtdr[j] = 0
      }
      else {
         outtime[j] = rowtime[j] - timeshift
         outnodes[j] = rownodes[j] - nodeshift
         outpt[j] = rowpt[j] - timeshift
         outf[j] = rowf[j] - nodeshift
         outpn[j] = rowpn[j] - nodeshift
         outdr[j] = rowdr[j] - nodeshift
         outtdr[j] = rowtdr[j] - nodeshift
      }
   }

   rowmethods = n
}

function compute_best(n,   j) {
   bestsolve = -1
   besttime = 1e100
   bestnodes = 1e100
   for (j = 1; j <= n; ++j) {
      if (rowsolved[j] > bestsolve)
         bestsolve = rowsolved[j]
      if (outtime[j] < besttime)
         besttime = outtime[j]
      if (outnodes[j] < bestnodes)
         bestnodes = outnodes[j]
   }
}

function same(a, b) {
   return (a - b < 0.000001 && b - a < 0.000001)
}

function print_metric(j) {
   if (rowsolved[j] == bestsolve)
      printf(" & \\textbf{%d}", rowsolved[j])
   else
      printf(" & %d", rowsolved[j])

   if (same(outtime[j], besttime))
      printf(" & \\textbf{%.1f}", round_to(outtime[j], 1))
   else
      printf(" & %.1f", round_to(outtime[j], 1))

   if (same(outnodes[j], bestnodes))
      printf(" & \\textbf{%d}", round(outnodes[j]))
   else
      printf(" & %d", round(outnodes[j]))
}

function finish_row() {
   printf(" \\\\\n")
}

function label_prob(prob) {
   if (prob == "CCRP")
      return "\\texttt{CCRP}"
   if (prob == "CCMPP")
      return "\\texttt{CCMPP}"
   if (prob == "CCLS")
      return "\\texttt{CCLS}"
   if (prob == "All")
      return "\\texttt{All}"
   return prob
}

function label_threshold(threshold) {
   if (threshold == 0)
      return "\\texttt{All}"
   return "\\texttt{$\\geq" threshold "$}"
}

function print_pt_value(j, bestpt) {
   if (same(outpt[j], bestpt))
      printf(" & \\textbf{%.1f}", round_to(outpt[j], 1))
   else
      printf(" & %.1f", round_to(outpt[j], 1))
}

function print_f_value(j, bestf) {
   if (same(outf[j], bestf))
      printf(" & \\textbf{%.1f}", round_to(outf[j], 1))
   else
      printf(" & %.1f", round_to(outf[j], 1))
}

function pn_text(v) {
   if (v > 0 && v < 1)
      return "$<$1"
   return sprintf("%d", round(v))
}

function print_table1_row(label, probfilter, threshold) {
   aggregate("BnC-MIX BnC-MIX-DI DB-OPF@14400", probfilter, threshold, 0, "main")
   compute_best(rowmethods)
   printf("%s%s & %3d", rowprefix, label, rown)
   print_metric(1)
   print_metric(2)
   print_metric(3)
   printf(" & %.1f & %.1f & %d", round_to(outpt[3], 1), round_to(outf[3], 1), round(outpn[3]))
   finish_row()
}

function table1_begin() {
   print "\\begin{table}[htbp]"
   print "\t\\small"
   print "\t\\addtolength{\\tabcolsep}{-2.2pt}"
   print "\t\\centering"
   print "\t\\caption{Performance comparison of the proposed dominance-based branching (with the overlap-oriented node pruning and variable fixing techniques) with the state-of-the-art approaches in Ruszczynski (2002) and Luedtke et al. (2010).}"
   print "\t\\begin{tabular*}{\\textwidth}{@{\\extracolsep\\fill}lrrrrrrrrrrrrr@{\\extracolsep\\fill}}"
   print "\t\t\\toprule"
   print "\t\t\\multirow{2}{*}{\\texttt{Probs}} & \\multirow{2}{*}{\\texttt{\\#}} &"
   print "\t\t\\multicolumn{3}{@{}c@{}}{\\texttt{BASE}} & \\multicolumn{3}{@{}c@{}}{\\texttt{BASE+DI}} & \\multicolumn{6}{@{}c@{}}{\\texttt{BASE+DB+OPF}} \\\\"
   print "\t\t\\cmidrule(l{4pt}r{3pt}){3-5} \\cmidrule(l{4pt}r{3pt}){6-8} \\cmidrule(l{4pt}r){9-14}"
   print "\t\t& &\\texttt{S} & \\texttt{T} & \\texttt{N} &\\texttt{S} & \\texttt{T} & \\texttt{N} & \\texttt{S} & \\texttt{T} & \\texttt{N} & \\texttt{PT} & \\texttt{F} & \\texttt{PN}  \\\\ "
   print "\t\t\\midrule"
}

function table1_end() {
   print "\t\t\\bottomrule"
   print "\t\\end{tabular*}"
   print "\t\\label{solver}"
   print "\\end{table}"
}

function table1_rows() {
   rowprefix = "\t\t"
   print_table1_row(label_prob("CCRP"), "CCRP", 0)
   print_table1_row(label_prob("CCMPP"), "CCMPP", 0)
   print_table1_row(label_prob("CCLS"), "CCLS", 0)
   print "\t\t\\midrule"
   print_table1_row(label_threshold(0), "All", 0)
   print_table1_row(label_threshold(10), "All", 10)
   print_table1_row(label_threshold(100), "All", 100)
   print_table1_row(label_threshold(1000), "All", 1000)
   rowprefix = ""
}

function print_table2_row(label, probfilter) {
   aggregate("BnC-MIX-DI BnC-MIX-sDI DB", probfilter, 0, 0, "main")
   compute_best(rowmethods)
   printf("%s%s & %3d", rowprefix, label, rown)
   print_metric(3)
   print_metric(2)
   printf(" & %.1f & %.1f", round_to(outdr[2], 1), round_to(outtdr[2], 1))
   print_metric(1)
   printf(" & %.1f & %.1f", round_to(outdr[1], 1), round_to(outtdr[1], 1))
   finish_row()
}

function print_table3_row(label, probfilter, threshold) {
   aggregate("DB DB-OPF@14400", probfilter, threshold, 0, "main")
   compute_best(rowmethods)
   bestpt = (outpt[1] < outpt[2]) ? outpt[1] : outpt[2]
   printf("%s%s & %3d", rowprefix, label, rown)
   print_metric(1)
   print_pt_value(1, bestpt)
   printf(" & %.1f", round_to(outf[1], 1))
   print_metric(2)
   print_pt_value(2, bestpt)
   printf(" & %.1f & %s", round_to(outf[2], 1), pn_text(outpn[2]))
   finish_row()
}

function print_table4_row(label, probfilter) {
   aggregate("DB-OPF@86400 DB-EOPF", probfilter, 0, 1, "main")
   compute_best(rowmethods)
   bestpt = (outpt[1] < outpt[2]) ? outpt[1] : outpt[2]
   bestf = (outf[1] > outf[2]) ? outf[1] : outf[2]
   printf("%s%s & %3d", rowprefix, label, rown)
   print_metric(1)
   print_pt_value(1, bestpt)
   print_f_value(1, bestf)
   printf(" & %s", pn_text(outpn[1]))
   print_metric(2)
   print_pt_value(2, bestpt)
   print_f_value(2, bestf)
   printf(" & %d", round(outpn[2]))
   finish_row()
}

function table2_begin() {
   print "\\begin{table}[htbp]"
   print "\t\\small"
   print "\t\\centering"
   print "\t\\addtolength{\\tabcolsep}{-3pt}"
   print "\t\\caption{Performance comparison of settings \\texttt{BASE+DB}, \\texttt{BASE+sDI}, and \\texttt{BASE+DI}.}"
   print "\t\\begin{tabular*}{\\textwidth}{@{\\extracolsep\\fill}lrrrrrrrrrrrrrr@{\\extracolsep\\fill}}"
   print "\t\t\\toprule"
   print "\t\t\\multirow{2}{*}{\\texttt{Probs}} & \\multirow{2}{*}{\\texttt{\\#}}"
   print "\t\t& \\multicolumn{3}{c}{\\texttt{BASE+DB}} & \\multicolumn{5}{c}{\\texttt{BASE+sDI}} & \\multicolumn{5}{c}{\\texttt{BASE+DI}} \\\\ "
   print "\t\t\\cmidrule(l{4pt}r{3pt}){3-5}\\cmidrule(l{4pt}r{3pt}){6-10} \\cmidrule(l{4pt}r{3pt}){11-15} &"
   print "\t\t& \\texttt{S} & \\texttt{T} & \\texttt{N} & \\texttt{S} & \\texttt{T} & \\texttt{N} & \\texttt{\\%DP} & \\texttt{\\%NDI} & \\texttt{S} & \\texttt{T} & \\texttt{N} & \\texttt{\\%DP} & \\texttt{\\%NDI} \\\\"
   print "\t\t\\midrule "
}

function table2_rows() {
   rowprefix = "\t\t"
   print_table2_row(label_prob("CCRP"), "CCRP")
   print_table2_row(label_prob("CCMPP"), "CCMPP")
   print_table2_row(label_prob("CCLS"), "CCLS")
   print "\t\t\\midrule"
   print_table2_row(label_prob("All"), "All")
   rowprefix = ""
}

function table2_end() {
   print "\t\t\\bottomrule"
   print "\t\\end{tabular*}"
   print "\t\\label{branch}"
   print "\\end{table}"
}

function table3_begin() {
   print "\\begin{table}[htbp]"
   print "\t \\small"
   print "\t\\centering"
   print "\t\\addtolength{\\tabcolsep}{-3pt}"
   print "\t\\caption{Performance comparison of settings \\texttt{BASE+DB} and \\texttt{BASE+DB+OPF}.}"
   print "\t\\begin{tabular*}{\\textwidth}{@{\\extracolsep\\fill}lrrrrrrrrrrrr@{\\extracolsep\\fill}}"
   print "\t\t\\toprule"
   print "\t\t\\multirow{2}{*}{\\texttt{Probs}} & \\multirow{2}{*}{\\texttt{\\#}} &"
   print "\t\t\\multicolumn{5}{c}{\\texttt{BASE+DB}} & \\multicolumn{6}{c}{\\texttt{BASE+DB+OPF}} \\\\"
   print "\t\t\\cmidrule{3-7} \t\\cmidrule{8-13} "
   print "\t\t& & \\texttt{S} & \\texttt{T} & \\texttt{N} &\\texttt{PT} & \\texttt{F} & \\texttt{S} & \\texttt{T} & \\texttt{N} &\\texttt{PT} & \\texttt{F} & \\texttt{PN} \\\\ "
   print "\t\t\\midrule"
}

function table3_rows() {
   rowprefix = "\t\t"
   print_table3_row(label_prob("CCRP"), "CCRP", 0)
   print_table3_row(label_prob("CCMPP"), "CCMPP", 0)
   print_table3_row(label_prob("CCLS"), "CCLS", 0)
   print "\t\t\\midrule"
   print_table3_row(label_threshold(0), "All", 0)
   print_table3_row(label_threshold(10), "All", 10)
   print_table3_row(label_threshold(100), "All", 100)
   print_table3_row(label_threshold(1000), "All", 1000)
   rowprefix = ""
}

function table3_end() {
   print "\t\t\\bottomrule"
   print "\t\\end{tabular*}"
   print "\t\\label{pruneandfix}"
   print "\\end{table}"
}

function table4_begin() {
   print "\\begin{table}[htbp]"
   print "\t \\small"
   print "\t\\centering"
   print "\t\\addtolength{\\tabcolsep}{-3pt}"
   print "\t\\caption{Performance comparison of settings \\texttt{BASE+DB+OPF} and \\texttt{BASE+DB+EOPF}.}"
   print "\t\\begin{tabular*}{\\textwidth}{@{\\extracolsep\\fill}lrrrrrrrrrrrrr@{\\extracolsep\\fill}}"
   print "\t\t\\toprule"
   print "\t\t\\multirow{2}{*}{\\texttt{Probs}} & \\multirow{2}{*}{\\texttt{\\#}} &"
   print "\t\t\\multicolumn{6}{c}{\\texttt{BASE+DB+OPF}} & \\multicolumn{6}{c}{\\texttt{BASE+DB+EOPF}} \\\\"
   print "\t\t\\cmidrule(l{4pt}r{3pt}){3-8} \\cmidrule(l{4pt}r{3pt}){9-14} &"
   print "\t\t& \\texttt{S} & \\texttt{T} & \\texttt{N} &\\texttt{PT} & \\texttt{F} & \\texttt{PN}  & \\texttt{S} & \\texttt{T} & \\texttt{N} &\\texttt{PT} & \\texttt{F}  & \\texttt{PN} \\\\ "
   print "\t\t\\midrule"
}

function table4_rows() {
   rowprefix = "\t\t"
   print_table4_row(label_prob("CCRP"), "CCRP")
   print_table4_row(label_prob("CCMPP"), "CCMPP")
   print_table4_row(label_prob("CCLS"), "CCLS")
   print "\t\t\\midrule"
   print_table4_row(label_prob("All"), "All")
   rowprefix = ""
}

function table4_end() {
   print "\t\t\\bottomrule"
   print "\t\\end{tabular*}\t\\label{anpfandenpf}"
   print "\\end{table}"
}

function print_table1() {
   if (prob == "" && !rows_only) {
      table1_begin()
      table1_rows()
      table1_end()
      return
   }

   print "% Table 1"
   if (prob != "") {
      if (prob == "All") {
         print_table1_row(label_threshold(0), "All", 0)
         print_table1_row(label_threshold(10), "All", 10)
         print_table1_row(label_threshold(100), "All", 100)
         print_table1_row(label_threshold(1000), "All", 1000)
      }
      else if (prob == "CCRP")
         print_table1_row(label_prob(prob), prob, 0)
      else if (prob == "CCMPP")
         print_table1_row(label_prob(prob), prob, 0)
      else if (prob == "CCLS")
         print_table1_row(label_prob(prob), prob, 0)
      else
         print_table1_row(prob, prob, 0)
      return
   }
   print_table1_row(label_prob("CCRP"), "CCRP", 0)
   print_table1_row(label_prob("CCMPP"), "CCMPP", 0)
   print_table1_row(label_prob("CCLS"), "CCLS", 0)
   print_table1_row(label_threshold(0), "All", 0)
   print_table1_row(label_threshold(10), "All", 10)
   print_table1_row(label_threshold(100), "All", 100)
   print_table1_row(label_threshold(1000), "All", 1000)
}

function print_table2() {
   if (prob == "" && !rows_only) {
      table2_begin()
      table2_rows()
      table2_end()
      return
   }

   print "% Table 2"
   if (prob != "") {
      print_table2_row(label_prob(prob), prob)
      return
   }
   print_table2_row(label_prob("CCRP"), "CCRP")
   print_table2_row(label_prob("CCMPP"), "CCMPP")
   print_table2_row(label_prob("CCLS"), "CCLS")
   print_table2_row(label_prob("All"), "All")
}

function print_table3() {
   if (prob == "" && !rows_only) {
      table3_begin()
      table3_rows()
      table3_end()
      return
   }

   print "% Table 3"
   if (prob != "") {
      if (prob == "All") {
         print_table3_row(label_threshold(0), "All", 0)
         print_table3_row(label_threshold(10), "All", 10)
         print_table3_row(label_threshold(100), "All", 100)
         print_table3_row(label_threshold(1000), "All", 1000)
      }
      else
         print_table3_row(label_prob(prob), prob, 0)
      return
   }
   print_table3_row(label_prob("CCRP"), "CCRP", 0)
   print_table3_row(label_prob("CCMPP"), "CCMPP", 0)
   print_table3_row(label_prob("CCLS"), "CCLS", 0)
   print_table3_row(label_threshold(0), "All", 0)
   print_table3_row(label_threshold(10), "All", 10)
   print_table3_row(label_threshold(100), "All", 100)
   print_table3_row(label_threshold(1000), "All", 1000)
}

function print_table4() {
   if (prob == "" && !rows_only) {
      table4_begin()
      table4_rows()
      table4_end()
      return
   }

   print "% Table 4"
   if (prob != "") {
      print_table4_row(label_prob(prob), prob)
      return
   }
   print_table4_row(label_prob("CCRP"), "CCRP")
   print_table4_row(label_prob("CCMPP"), "CCMPP")
   print_table4_row(label_prob("CCLS"), "CCLS")
   print_table4_row(label_prob("All"), "All")
}

function init_table5(   m, j) {
   nm = split("10 20 30 50 100 150", mlist, " ")
   for (m = 1; m <= nm; ++m) {
      for (j = 1; j <= 4; ++j) {
         msolved[m, j] = 0
         mavetime[m, j] = 1
         mavenodes[m, j] = 1
      }
      mavedr[m] = 1
   }
}

function table5_key(inst, eps) {
   return inst "-" eps "-mpp"
}

function table5_begin() {
   print "\\begin{table}[htbp]"
   print "\t\\small"
   print "\t\\centering"
   print "\t\\caption{Performance comparison of settings \\texttt{BASE}, \\texttt{BASE+sDI}, \\texttt{BASE+DB}, and \\texttt{BASE+DB+OPF} with different dimensions $m$ of the random vector $\\xi$ on the \\texttt{CCMPP} instances.}"
   print "\t\\addtolength{\\tabcolsep}{1pt}"
   print "\t\\begin{tabular*}{\\textwidth}{@{\\extracolsep\\fill}llrrrrrr@{\\extracolsep\\fill}}"
   print "\t\t\\toprule"
}

function table5_end() {
   print "\t\t\\bottomrule"
   print "\t\\end{tabular*}\\label{dimension}"
   print "\\end{table}"
}

function print_table5_header_row() {
   printf("%s& $m$", rowprefix)
   for (m = 1; m <= nm; ++m)
      printf(" & %s", mlist[m])
   finish_row()
}

function print_table5_dr_row() {
   printf("%s& \\texttt{\\%%DP}", rowprefix)
   for (m = 1; m <= nm; ++m)
      printf(" & %.1f", round_to(moutdr[m], 1))
   finish_row()
}

function print_table5_num_row() {
   printf("%s& \\texttt{\\#}", rowprefix)
   for (m = 1; m <= nm; ++m)
      printf(" & %d", mnumt[m])
   finish_row()
}

function print_table5_value_row(method_label, metric_label, values, j, is_delta, is_ratio,   m) {
   printf("%s%s & %s", rowprefix, method_label, metric_label)
   for (m = 1; m <= nm; ++m) {
      if (is_delta)
         printf(" & %d", msolved[m, j] - msolved[m, 1])
      else if (is_ratio) {
         if (values == "time")
            printf(" & %.1f", round_to(ratio(mouttime[m, 1], mouttime[m, j]), 1))
         else
            printf(" & %.1f", round_to(ratio(moutnodes[m, 1], moutnodes[m, j]), 1))
      }
      else if (values == "solved")
         printf(" & %d", msolved[m, j])
      else if (values == "time")
         printf(" & %.1f", round_to(mouttime[m, j], 1))
      else
         printf(" & %d", round(moutnodes[m, j]))
   }
   finish_row()
}

function print_table5(   I, J, K, ni, nj, nk, m, ii, jj, kk, inst, p, methods, anyok, numt, numn, j, key, val) {
   init_table5()
   methods[1] = "BnC-MIX"
   methods[2] = "BnC-MIX-sDI"
   methods[3] = "DB"
   methods[4] = "DB-OPF@14400"
   ni = split("1000 2000 3000", I, " ")
   nj = split("0 1 2 3 4", J, " ")
   nk = split("0.05 0.1 0.2", K, " ")

   for (m = 1; m <= nm; ++m) {
      numt = 0
      numn = 0
      for (ii = 1; ii <= ni; ++ii) {
         for (jj = 1; jj <= nj; ++jj) {
            for (kk = 1; kk <= nk; ++kk) {
               inst = mlist[m] "-" I[ii] "-" J[jj]
               p = table5_key(inst, K[kk])

               anyok = 0
               for (j = 1; j <= 4; ++j) {
                  key = p SUBSEP methods[j]
                  if (status[key] == "ok") {
                     anyok = 1
                     ++msolved[m, j]
                  }
               }

               if (anyok) {
                  for (j = 1; j <= 4; ++j) {
                     key = p SUBSEP methods[j]
                     val = min(timev[key], timelimit)
                     mavetime[m, j] = gmean_update(mavetime[m, j], val, timeshift, numt)
                     mavenodes[m, j] = gmean_update(mavenodes[m, j], nodes[key], nodeshift, numn)
                  }
                  key = p SUBSEP methods[3]
                  mavedr[m] = gmean_update(mavedr[m], dr[key], nodeshift, numt)
                  ++numt
                  ++numn
               }
            }
         }
      }
      mnumt[m] = numt
      mnumn[m] = numn
   }

   for (m = 1; m <= nm; ++m) {
      moutdr[m] = (mnumt[m] == 0) ? 0 : mavedr[m] - nodeshift
      for (j = 1; j <= 4; ++j) {
         mouttime[m, j] = (mnumt[m] == 0) ? 0 : mavetime[m, j] - timeshift
         moutnodes[m, j] = (mnumn[m] == 0) ? 0 : mavenodes[m, j] - nodeshift
      }
   }

   if (!rows_only)
      table5_begin()
   else
      print "% Table 5"

   rowprefix = rows_only ? "" : "\t\t"
   print_table5_header_row()
   if (!rows_only)
      print "\t\t\\midrule"
   print_table5_dr_row()
   print_table5_num_row()
   if (!rows_only)
      print "\t\t\\midrule"
   print_table5_value_row("\\texttt{BASE}", "\\texttt{S}", "solved", 1, 0, 0)
   print_table5_value_row("", "\\texttt{T}", "time", 1, 0, 0)
   print_table5_value_row("", "\\texttt{N}", "nodes", 1, 0, 0)
   if (!rows_only)
      print "\t\t\\midrule"
   print_table5_value_row("\\texttt{BASE+sDI}", "\\texttt{$\\Delta$S}", "", 2, 1, 0)
   print_table5_value_row("", "\\texttt{RT}", "time", 2, 0, 1)
   print_table5_value_row("", "\\texttt{RN}", "nodes", 2, 0, 1)
   if (!rows_only)
      print "\t\t\\midrule"
   print_table5_value_row("\\texttt{BASE+DB}", "\\texttt{$\\Delta$S}", "", 3, 1, 0)
   print_table5_value_row("", "\\texttt{RT}", "time", 3, 0, 1)
   print_table5_value_row("", "\\texttt{RN}", "nodes", 3, 0, 1)
   if (!rows_only)
      print "\t\t\\midrule"
   print_table5_value_row("\\texttt{BASE+DB+OPF}", "\\texttt{$\\Delta$S}", "", 4, 1, 0)
   print_table5_value_row("", "\\texttt{RT}", "time", 4, 0, 1)
   print_table5_value_row("", "\\texttt{RN}", "nodes", 4, 0, 1)
   rowprefix = ""

   if (!rows_only)
      table5_end()
}

function g_objective_to_3(value, rounded) {
   rounded = round_to(value, 3)
   if (rounded == 0)
      rounded = 0
   return sprintf("%.3f", rounded)
}

function g_check_optimal_objectives(   methods, n, i, j, p, method, key, rounded, reference_found, reference_value, reference_method, checked, checked_instances, errors) {
   methods = "BASE BASE+sDI"
   n = split(methods, g_objective_method, " ")
   checked = 0
   checked_instances = 0
   errors = 0

   for (i = 1; i <= g_probnum; ++i) {
      p = g_problist[i]
      reference_found = 0

      for (j = 1; j <= n; ++j) {
         method = g_objective_method[j]
         key = p SUBSEP method
         if (g_probfile[key] != 1 || g_status[key] != "ok")
            continue

         ++checked
         if (!(key in g_objective_seen)) {
            printf "ERROR optimal Gurobi result has no ObjectiveValue: instance=%s setting=%s\n", p, method > "/dev/stderr"
            ++errors
            continue
         }

         rounded = g_objective_to_3(g_objective[key])
         if (!reference_found) {
            reference_found = 1
            reference_value = rounded
            reference_method = method
            ++checked_instances
         }
         else if (rounded != reference_value) {
            printf "ERROR Gurobi objective mismatch at 3 decimal places: instance=%s %s=%s %s=%s\n", p, reference_method, reference_value, method, rounded > "/dev/stderr"
            ++errors
         }
      }
   }

   return errors
}

function g_print_metric(j) {
   if (rowsolved[j] == bestsolve)
      printf(" & \\textbf{%d}", rowsolved[j])
   else
      printf(" & %d", rowsolved[j])

   if (same(outtime[j], besttime))
      printf(" & \\textbf{%.1f}", round_to(outtime[j], 1))
   else
      printf(" & %.1f", round_to(outtime[j], 1))

   if (same(outnodes[j], bestnodes))
      printf(" & \\textbf{%d}", round(outnodes[j]))
   else
      printf(" & %d", round(outnodes[j]))
}

function table6_eps_allowed(family, eps) {
   if (family == "CCRP")
      return same(eps, 0.10) || same(eps, 0.15) || same(eps, 0.20)
   return same(eps, 0.05) || same(eps, 0.10) || same(eps, 0.20)
}

function table6_aggregate(probfilter, targeteps,   i, j, p, family, gbase, gsdi, sopf, anyok, key, data, node_data) {
   rown = 0
   for (j = 1; j <= 3; ++j) {
      rowsolved[j] = 0
      rowtime[j] = 1
      rownodes[j] = 1
   }

   for (i = 1; i <= g_probnum; ++i) {
      p = g_problist[i]
      family = g_probtype[p]
      if (probfilter != "All" && family != probfilter)
         continue
      if (!table6_eps_allowed(family, g_epsvalue[p]))
         continue
      if (targeteps != "All" && !same(g_epsvalue[p], targeteps))
         continue
      if (family == "CCMPP" && g_mvalue[p] != 10 && g_mvalue[p] != 20 && g_mvalue[p] != 30)
         continue

      gbase = p SUBSEP "BASE"
      gsdi = p SUBSEP "BASE+sDI"
      sopf = p SUBSEP "DB-OPF@14400"
      if (g_probfile[gbase] != 1 || g_probfile[gsdi] != 1 || probfile[sopf] != 1)
         continue

      anyok = (g_status[gbase] == "ok" || g_status[gsdi] == "ok" || status[sopf] == "ok")
      if (!anyok)
         continue

      for (j = 1; j <= 3; ++j) {
         if (j == 1) {
            key = gbase
            if (g_status[key] == "ok") {
               ++rowsolved[j]
               data = min(g_timev[key], timelimit)
            }
            else
               data = timelimit
            node_data = g_nodes[key]
         }
         else if (j == 2) {
            key = gsdi
            if (g_status[key] == "ok") {
               ++rowsolved[j]
               data = min(g_timev[key], timelimit)
            }
            else
               data = timelimit
            node_data = g_nodes[key]
         }
         else {
            key = sopf
            if (status[key] == "ok") {
               ++rowsolved[j]
               data = min(timev[key], timelimit)
            }
            else
               data = timelimit
            node_data = nodes[key]
         }

         rowtime[j] = gmean_update(rowtime[j], data, timeshift, rown)
         rownodes[j] = gmean_update(rownodes[j], node_data, nodeshift, rown)
      }
      ++rown
   }

   for (j = 1; j <= 3; ++j) {
      if (rown == 0) {
         outtime[j] = 0
         outnodes[j] = 0
      }
      else {
         outtime[j] = rowtime[j] - timeshift
         outnodes[j] = rownodes[j] - nodeshift
      }
   }
   compute_best(3)
}

function table6_begin() {
   print "\\begin{table}[htbp]"
   print "\t\\small"
   print "\t\\centering"
   print "\t\\renewcommand{\\arraystretch}{0.9}"
   print "\t\\addtolength{\\tabcolsep}{-1pt}"
   print "\t\\caption{Performance comparison of settings \\texttt{G-BASE}, \\texttt{G-BASE+sDI}, and \\texttt{BASE+DB+OPF}.}"
   print "\t\\begin{tabular*}{\\textwidth}{@{\\extracolsep\\fill}llrrrrrrrrrr@{\\extracolsep\\fill}}"
   print "\t\t\\toprule"
   print "\t\t\\multirow{2}{*}{\\texttt{Probs}} & \\multirow{2}{*}{$\\epsilon$} & \\multirow{2}{*}{\\texttt{\\#}}"
   print "\t\t& \\multicolumn{3}{c}{\\texttt{G-BASE}} & \\multicolumn{3}{c}{\\texttt{G-BASE+sDI}} & \\multicolumn{3}{c}{\\texttt{BASE+DB+OPF}} \\\\"
   print "\t\t\\cmidrule(l{4pt}r{3pt}){4-6} \\cmidrule(l{4pt}r{3pt}){7-9} \\cmidrule(l{4pt}r{3pt}){10-12}"
   print "\t\t& & & \\texttt{S} & \\texttt{T} & \\texttt{N} & \\texttt{S} & \\texttt{T} & \\texttt{N} & \\texttt{S} & \\texttt{T} & \\texttt{N} \\\\"
   print "\t\t\\midrule"
}

function table6_end() {
   print "\t\t\\bottomrule"
   print "\t\\end{tabular*}"
   print "\t\\label{gurobi-scip-three-problems}"
   print "\\end{table}"
}

function print_table6_row(probfilter, targeteps, firstrow, problabel, epslabel) {
   table6_aggregate(probfilter, targeteps)
   if (probfilter == "All")
      problabel = label_prob(probfilter)
   else
      problabel = firstrow ? "\\multirow{4}{*}{" label_prob(probfilter) "}" : ""
   if (targeteps == "All")
      epslabel = (probfilter == "All") ? "" : "\\texttt{All}"
   else
      epslabel = sprintf("%.2f", targeteps)
   if (probfilter == "All")
      printf("\t\t%s & & %d", problabel, rown)
   else
      printf("\t\t%s & %s & %d", problabel, epslabel, rown)
   g_print_metric(1)
   g_print_metric(2)
   g_print_metric(3)
   finish_row()
}

function print_table6_problem(probfilter,   epslist, neps, e) {
   if (probfilter == "CCRP")
      neps = split("0.10 0.15 0.20", epslist, " ")
   else
      neps = split("0.05 0.10 0.20", epslist, " ")

   for (e = 1; e <= neps; ++e)
      print_table6_row(probfilter, epslist[e] + 0, e == 1)
   if (!rows_only)
      print "\t\t\\cmidrule{2-12}"
   print_table6_row(probfilter, "All", 0)
}

function print_table6() {
   if (!rows_only)
      table6_begin()
   else
      print "% Table 6"

   if (prob != "" && prob != "All")
      print_table6_problem(prob)
   else {
      print_table6_problem("CCRP")
      if (!rows_only)
         print "\t\t\\midrule"
      print_table6_problem("CCMPP")
      if (!rows_only)
         print "\t\t\\midrule"
      print_table6_problem("CCLS")
   }

   if (!rows_only)
      table6_end()
}

function ratio(a, b) {
   if (b == 0)
      return 0
   return a / b
}

function round(x) {
   if (x < 0)
      return int(x - 0.5)
   return int(x + 0.5)
}

function round_to(x, digits, scale, i) {
   scale = 1
   for (i = 0; i < digits; ++i)
      scale *= 10
   return round(x * scale) / scale
}

function prob_suffix(prob) {
   if (prob == "CCRP")
      return "rpp"
   if (prob == "CCMPP")
      return "mpp"
   if (prob == "CCLS")
      return "lsp"
   return "all"
}

function parse_detail_instance(prob, a, len) {
   len = split(prob, a, "-")
   detail_z = ""
   if (len == 6) {
      detail_z = a[1]
      detail_m = a[2]
      detail_n = a[3]
      detail_seed = a[4]
      detail_eps = a[5]
      detail_suffix = a[6]
      return 1
   }
   if (len == 5) {
      detail_m = a[1]
      detail_n = a[2]
      detail_seed = a[3]
      detail_eps = a[4]
      detail_suffix = a[5]
      return 1
   }
   return 0
}

function detail_sort_key(prob) {
   if (!parse_detail_instance(prob))
      return prob
   return sprintf("%010.3f|%010.3f|%010.3f|%010.5f|%010.3f|%s", detail_z + 0, detail_m + 0, detail_n + 0, detail_eps + 0, detail_seed + 0, prob)
}
function insert_detail_row(prob, sortkey, pos) {
   pos = ++detailrows
   while (pos > 1 && detailsort[pos - 1] > sortkey) {
      detailsort[pos] = detailsort[pos - 1]
      detailrow[pos] = detailrow[pos - 1]
      --pos
   }
   detailsort[pos] = sortkey
   detailrow[pos] = prob
}

function detail_key_available(key) {
   if (key in probfile)
      return probfile[key] == 1
   return (key in badfile)
}

function collect_detail_rows(probfilter, method_string, family,   n, i, j, prob, method, key, present) {
   n = split(method_string, detailmethod, " ")
   detailrows = 0
   for (i = 1; i <= probnum; ++i) {
      prob = problist[i]
      if (!eligible(prob, probfilter, family))
         continue
      if (!parse_detail_instance(prob))
         continue

      present = 1
      for (j = 1; j <= n; ++j) {
         method = detailmethod[j]
         key = prob SUBSEP method
         if (!detail_key_available(key)) {
            present = 0
            break
         }
      }
      if (!present)
         continue

      insert_detail_row(prob, detail_sort_key(prob))
   }
   return detailrows
}

function fmt_time_key(key, limit, g) {
   if (key in badfile)
      return "--"
   if (!(key in probfile))
      return "--"
   if (timev[key] >= limit) {
      g = gaps[key]
      if (g < 0.1)
         return "($<$0.1)"
      return sprintf("(%.1f)", round_to(g, 1))
   }
   return sprintf("%.1f", round_to(timev[key], 1))
}

function fmt_small0(v) {
   if (v == 0)
      return "0.0"
   if (v > 0 && v < 0.1)
      return "$<$0.1"
   return sprintf("%.1f", round_to(v, 1))
}

function fmt_pt_key(key, limit, dash_on_timeout) {
   if (key in badfile)
      return "--"
   if (!(key in probfile))
      return "--"
   if (dash_on_timeout && timev[key] >= limit && chancetime[key] >= 0.1)
      return "--"
   if (chancetime[key] < 0.1)
      return "$<$0.1"
   return sprintf("%.1f", round_to(chancetime[key], 1))
}

function fmt_f_key(key) {
   if (key in badfile)
      return "--"
   if (!(key in probfile) || nodes[key] <= 0)
      return "0"
   return sprintf("%d", round(detaildomreds[key] / nodes[key]))
}

function fmt_pn_key(key) {
   if (key in badfile)
      return "--"
   if (!(key in probfile))
      return "0"
   return sprintf("%d", cutoffs[key])
}

function detail_time_node(prob, method, limit, key) {
   key = prob SUBSEP method
   if (key in badfile || !(key in probfile)) {
      printf(" & -- & --")
      return
   }
   printf(" & %s & %d", fmt_time_key(key, limit), nodes[key])
}

function detail_pt_f(prob, method, limit, dash_on_timeout, key) {
   key = prob SUBSEP method
   printf(" & %s & %s", fmt_pt_key(key, limit, dash_on_timeout), fmt_f_key(key))
}

function detail_dr_pair(prob, method, key) {
   key = prob SUBSEP method
   if (key in badfile || !(key in probfile)) {
      printf(" & -- & --")
      return
   }
   printf(" & %s & %s", fmt_dr_key(key), fmt_small0(truedr[key]))
}

function fmt_dr_key(key) {
   if (key in badfile || !(key in probfile))
      return "--"
   return fmt_small0(dr[key])
}

function detail_prob_from_suffix(suffix) {
   if (suffix == "rpp")
      return "CCRP"
   if (suffix == "lsp")
      return "CCLS"
   return "CCMPP"
}

function detail_first_header(probfilter) {
   if (probfilter == "CCRP")
      return "$(|\\mathcal{I}|,|\\mathcal{J}|)$"
   return "$T$"
}

function detail_first_value(probfilter) {
   if (probfilter == "CCRP" && detail_z != "")
      return "(" detail_z "," detail_m ")"
   return detail_m
}

function detail_first_changed(probfilter) {
   if (!detail_printed)
      return 1
   if (probfilter == "CCRP")
      return detail_z != detail_last_z || detail_m != detail_last_m
   return detail_m != detail_last_m
}

function detail_row_start(prob, colcount, probfilter, firstcell, ncell, epscell, firstchanged, nchanged, epschanged) {
   parse_detail_instance(prob)
   probfilter = detail_prob_from_suffix(detail_suffix)
   firstchanged = detail_first_changed(probfilter)
   nchanged = firstchanged || detail_n != detail_last_n
   epschanged = nchanged || detail_eps != detail_last_eps

   if (detail_printed) {
      if (firstchanged)
         print "\t\t\\cmidrule(r){1-" colcount "}"
      else if (detail_n != detail_last_n)
         print "\t\t\\cmidrule(r){2-" colcount "}"
      else if (detail_eps != detail_last_eps)
         print "\t\t\\cmidrule(r){3-" colcount "}"
   }

   firstcell = firstchanged ? detail_first_value(probfilter) : ""
   ncell = nchanged ? detail_n : ""
   epscell = epschanged ? detail_eps : ""

   printf("\t\t%s & %s & %s & %s", firstcell, ncell, epscell, detail_seed)
   detail_last_z = detail_z
   detail_last_m = detail_m
   detail_last_n = detail_n
   detail_last_eps = detail_eps
   detail_printed = 1
}
function prob_text(prob) {
   if (prob == "CCRP")
      return "\\texttt{CCRP}"
   if (prob == "CCMPP")
      return "\\texttt{CCMPP}"
   if (prob == "CCLS")
      return "\\texttt{CCLS}"
   return "\\texttt{" prob "}"
}

function detail_caption(kind, probfilter, p) {
   p = prob_text(probfilter)
   if (kind == "one")
      return "Detailed computational results of settings \\texttt{BASE+DB}, \\texttt{BASE+sDI}, and \\texttt{BASE+DI} on the instances in testset " p "."
   if (kind == "two")
      return "Detailed computational results of settings \\texttt{BASE+DB} and \\texttt{BASE+DB+OPF} on the instances in testset " p "."
   if (kind == "three")
      return "Detailed computational results of settings \\texttt{BASE+DB+OPF} and \\texttt{BASE+DB+EOPF} on the instances in testset " p ". ``--\" under column \\texttt{PT} denotes that the instance was not solved within 24 hours."
   if (kind == "four")
      return "Detailed computational results of settings \\texttt{BASE}, \\texttt{BASE+DI}, and \\texttt{BASE+DB+OPF} on the instances in testset " p "."
   return "Detailed computational results of settings \\texttt{BASE}, \\texttt{BASE+sDI}, \\texttt{BASE+DB}, and \\texttt{BASE+DB+OPF} on the instances in testset \\texttt{CCMPP}."
}

function detail_stretch(kind, probfilter) {
   if (kind == "three" && probfilter == "CCLS")
      return "0.835"
   if (kind == "three")
      return "0.84"
   return "0.87"
}

function detail_tabcolsep(kind) {
   if (kind == "one" || kind == "testm")
      return "1.8pt"
   if (kind == "two" || kind == "three")
      return "4pt"
   return "5pt"
}

function detail_common_begin(colspec, caption, label, kind, probfilter, colcount) {
   print "{"
   print "\t\\centering"
   print "\t\\small"
   print "\t\\setlength{\\tabcolsep}{" detail_tabcolsep(kind) "}"
   print "\t\\renewcommand{\\arraystretch}{" detail_stretch(kind, probfilter) "}"
   print "\t\\begin{longtable}{" colspec "}"
   print "\t\t\\caption{" caption "} \\label{" label "} \\\\"
   print "\t\t\\specialrule{0.5pt}{0pt}{3pt}"
}

function detail_common_head_end(colcount) {
   print "\t\t\\specialrule{0.5pt}{1pt}{3pt}"
   print "\t\t\\endhead"
   print "\t\t\\specialrule{0.5pt}{1pt}{1pt}"
   print "\t\t\\multicolumn{" colcount "}{r}{continued on next page}\\\\"
   print "\t\t\\specialrule{0.5pt}{1pt}{0pt}"
   print "\t\t\\endfoot"
   print "\t\t\\specialrule{0.5pt}{3pt}{0pt}"
   print "\t\t\\endlastfoot"
}

function detail_common_end() {
   print "\t\\end{longtable}"
   print "}"
}

function detail_num_label() {
   return "\\texttt{No.}"
}

function detail_tg_label() {
   return "\\texttt{T(G\\,\\%)}"
}

function detail_four_begin(probfilter, s) {
   s = prob_suffix(probfilter)
   detail_common_begin("cccc@{\\,}rrrrrrrrr", detail_caption("four", probfilter), "tablefour:detail-" s, "four", probfilter, 13)
   print "\t\t\\multirow{2}{*}{" detail_first_header(probfilter) "} & \\multirow{2}{*}{$n$} & \\multirow{2}{*}{$\\epsilon$} & \\multirow{2}{*}{" detail_num_label() "} &"
   print "\t\t\\multicolumn{2}{c}{\\texttt{BASE}} & \\multicolumn{2}{c}{\\texttt{BASE+DI}} & \\multicolumn{5}{c}{\\texttt{BASE+DB+OPF}} \\\\"
   print "\t\t\\cmidrule(r){5-6} \\cmidrule(r){7-8} \\cmidrule(r){9-13} & & & & " detail_tg_label() " & \\texttt{N} & " detail_tg_label() " & \\texttt{N} & " detail_tg_label() " & \\texttt{N} & \\texttt{PT} & \\texttt{F} & \\texttt{PN} \\\\"
   detail_common_head_end(13)
}

function print_detail_four(probfilter,   r, prob, key) {
   collect_detail_rows(probfilter, "BnC-MIX BnC-MIX-DI DB-OPF@14400", "main")
   detail_four_begin(probfilter)
   detail_printed = 0
   for (r = 1; r <= detailrows; ++r) {
      prob = detailrow[r]
      detail_row_start(prob, 13)
      detail_time_node(prob, "BnC-MIX", timelimit)
      detail_time_node(prob, "BnC-MIX-DI", timelimit)
      detail_time_node(prob, "DB-OPF@14400", timelimit)
      key = prob SUBSEP "DB-OPF@14400"
      printf(" & %s & %s & %s \\\\\n", fmt_pt_key(key, timelimit, 0), fmt_f_key(key), fmt_pn_key(key))
   }
   detail_common_end()
}

function detail_one_begin(probfilter, s) {
   s = prob_suffix(probfilter)
   detail_common_begin("cccc@{\\,}rrrrrrrrrrrr", detail_caption("one", probfilter), "tableone:detail-" s, "one", probfilter, 16)
   print "\t\t\\multirow{2}{*}{" detail_first_header(probfilter) "} & \\multirow{2}{*}{$n$} & \\multirow{2}{*}{$\\epsilon$} & \\multirow{2}{*}{" detail_num_label() "} &"
   print "\t\t\\multicolumn{4}{c}{\\texttt{BASE+DB}} & \\multicolumn{4}{c}{\\texttt{BASE+sDI}} & \\multicolumn{4}{c}{\\texttt{BASE+DI}} \\\\"
   print "\t\t\\cmidrule(r){5-8} \\cmidrule(r){9-12} \\cmidrule(r){13-16} & & & & " detail_tg_label() " & \\texttt{N} & \\texttt{PT} & \\texttt{F} & " detail_tg_label() " & \\texttt{N} & \\texttt{\\%DP} & \\texttt{\\%NDI} & " detail_tg_label() " & \\texttt{N} & \\texttt{\\%DP} & \\texttt{\\%NDI} \\\\"
   detail_common_head_end(16)
}

function print_detail_one(probfilter,   r, prob) {
   collect_detail_rows(probfilter, "DB BnC-MIX-sDI BnC-MIX-DI", "main")
   detail_one_begin(probfilter)
   detail_printed = 0
   for (r = 1; r <= detailrows; ++r) {
      prob = detailrow[r]
      detail_row_start(prob, 16)
      detail_time_node(prob, "DB", timelimit)
      detail_pt_f(prob, "DB", timelimit, 0)
      detail_time_node(prob, "BnC-MIX-sDI", timelimit)
      detail_dr_pair(prob, "BnC-MIX-sDI")
      detail_time_node(prob, "BnC-MIX-DI", timelimit)
      detail_dr_pair(prob, "BnC-MIX-DI")
      printf(" \\\\\n")
   }
   detail_common_end()
}

function detail_two_begin(probfilter, s) {
   s = prob_suffix(probfilter)
   detail_common_begin("cccc@{\\,}rrrrrrrrr", detail_caption("two", probfilter), "tabletwo:detail-" s, "two", probfilter, 13)
   print "\t\t\\multirow{2}{*}{" detail_first_header(probfilter) "} & \\multirow{2}{*}{$n$} & \\multirow{2}{*}{$\\epsilon$} & \\multirow{2}{*}{" detail_num_label() "} &"
   print "\t\t\\multicolumn{4}{c}{\\texttt{BASE+DB}} & \\multicolumn{5}{c}{\\texttt{BASE+DB+OPF}} \\\\"
   print "\t\t\\cmidrule(r){5-8} \\cmidrule(r){9-13} & & & & " detail_tg_label() " & \\texttt{N} & \\texttt{PT} & \\texttt{F} & " detail_tg_label() " & \\texttt{N} & \\texttt{PT} & \\texttt{F} & \\texttt{PN} \\\\"
   detail_common_head_end(13)
}

function print_detail_two(probfilter,   r, prob, key) {
   collect_detail_rows(probfilter, "DB DB-OPF@14400", "main")
   detail_two_begin(probfilter)
   detail_printed = 0
   for (r = 1; r <= detailrows; ++r) {
      prob = detailrow[r]
      detail_row_start(prob, 13)
      detail_time_node(prob, "DB", timelimit)
      detail_pt_f(prob, "DB", timelimit, 0)
      detail_time_node(prob, "DB-OPF@14400", timelimit)
      key = prob SUBSEP "DB-OPF@14400"
      printf(" & %s & %s & %s \\\\\n", fmt_pt_key(key, timelimit, 0), fmt_f_key(key), fmt_pn_key(key))
   }
   detail_common_end()
}

function detail_three_begin(probfilter, s) {
   s = prob_suffix(probfilter)
   detail_common_begin("cccc@{\\,}rrrrrrrrr", detail_caption("three", probfilter), "tablethree:detail-" s, "three", probfilter, 13)
   print "\t\t\\multirow{2}{*}{" detail_first_header(probfilter) "} & \\multirow{2}{*}{$n$} & \\multirow{2}{*}{$\\epsilon$} & \\multirow{2}{*}{" detail_num_label() "} &"
   print "\t\t\\multicolumn{5}{c}{\\texttt{BASE+DB+OPF}} & \\multicolumn{4}{c}{\\texttt{BASE+DB+EOPF}} \\\\"
   print "\t\t\\cmidrule(r){5-9} \\cmidrule(r){10-13} & & & & " detail_tg_label() " & \\texttt{N} & \\texttt{PT} & \\texttt{F} & \\texttt{PN} & " detail_tg_label() " & \\texttt{N} & \\texttt{PT} & \\texttt{F} \\\\"
   detail_common_head_end(13)
}

function print_detail_three(probfilter,   r, prob, key) {
   collect_detail_rows(probfilter, "DB-OPF@86400 DB-EOPF", "main")
   detail_three_begin(probfilter)
   detail_printed = 0
   for (r = 1; r <= detailrows; ++r) {
      prob = detailrow[r]
      detail_row_start(prob, 13)
      detail_time_node(prob, "DB-OPF@86400", timelimit2)
      key = prob SUBSEP "DB-OPF@86400"
      printf(" & %s & %s & %s", fmt_pt_key(key, timelimit2, 1), fmt_f_key(key), fmt_pn_key(key))
      detail_time_node(prob, "DB-EOPF", timelimit2)
      detail_pt_f(prob, "DB-EOPF", timelimit2, 1)
      printf(" \\\\\n")
   }
   detail_common_end()
}

function detail_testm_begin(s) {
   s = prob_suffix("CCMPP")
   detail_common_begin("cccc@{\\,}rrrrrrrrr", detail_caption("testm", "CCMPP"), "tabletestm:detail-" s, "testm", "CCMPP", 13)
   print "\t\t\\multirow{2}{*}{" detail_first_header("CCMPP") "} & \\multirow{2}{*}{$n$} & \\multirow{2}{*}{$\\epsilon$} & \\multirow{2}{*}{" detail_num_label() "} &"
   print "\t\t\\multicolumn{2}{c}{\\texttt{BASE}} & \\multicolumn{2}{c}{\\texttt{BASE+sDI}} & \\multicolumn{2}{c}{\\texttt{BASE+DB}} & \\multicolumn{2}{c}{\\texttt{BASE+DB+OPF}} & \\multirow{2}{*}{\\texttt{\\%DP}} \\\\"
   print "\t\t\\cmidrule(r){5-6} \\cmidrule(r){7-8} \\cmidrule(r){9-10} \\cmidrule(r){11-12} & & & & " detail_tg_label() " & \\texttt{N} & " detail_tg_label() " & \\texttt{N} & " detail_tg_label() " & \\texttt{N} & " detail_tg_label() " & \\texttt{N} & \\\\"
   detail_common_head_end(13)
}

function print_detail_testm(   r, prob, key) {
   collect_detail_rows("CCMPP", "BnC-MIX BnC-MIX-sDI DB DB-OPF@14400", "all")
   detail_testm_begin()
   detail_printed = 0
   for (r = 1; r <= detailrows; ++r) {
      prob = detailrow[r]
      detail_row_start(prob, 13)
      detail_time_node(prob, "BnC-MIX", timelimit)
      detail_time_node(prob, "BnC-MIX-sDI", timelimit)
      detail_time_node(prob, "DB", timelimit)
      detail_time_node(prob, "DB-OPF@14400", timelimit)
      key = prob SUBSEP "DB"
      printf(" & %s \\\\\n", fmt_dr_key(key))
   }
   detail_common_end()
}

function print_detail_tables() {
   print "% 5.1"
   print_detail_four("CCRP")
   print ""
   print_detail_four("CCMPP")
   print ""
   print_detail_four("CCLS")
   print ""
   print "% 5.2"
   print_detail_one("CCRP")
   print ""
   print_detail_one("CCMPP")
   print ""
   print_detail_one("CCLS")
   print ""
   print "% 5.3-1"
   print_detail_two("CCRP")
   print ""
   print_detail_two("CCMPP")
   print ""
   print_detail_two("CCLS")
   print ""
   print "% 5.3-2"
   print_detail_three("CCRP")
   print ""
   print_detail_three("CCMPP")
   print ""
   print_detail_three("CCLS")
   print ""
   print "% 5.4"
   print_detail_testm()
}

END {
   finish_uncommitted_file()
   g_finish_file()

   if (fatal)
      exit 2

   if ((table == "6" || table == "all") && g_probnum > 0) {
      if (g_check_optimal_objectives() > 0)
         exit 3
   }

   if (table == "1")
      print_table1()
   else if (table == "2")
      print_table2()
   else if (table == "3")
      print_table3()
   else if (table == "4")
      print_table4()
   else if (table == "5")
      print_table5()
   else if (table == "6")
      print_table6()
   else if (table == "detail" || detail)
      print_detail_tables()
   else {
      print_table1()
      print ""
      print_table2()
      print ""
      print_table3()
      print ""
      print_table4()
      print ""
      print_table5()
      if (g_probnum > 0 && probnum > 0) {
         print ""
         print_table6()
      }
   }

   if (g_incomplete_files > 0)
      printf "WARN ignored %d incomplete Gurobi output files\n", g_incomplete_files > "/dev/stderr"
}
