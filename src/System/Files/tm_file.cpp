
/******************************************************************************
* MODULE     : tm_file.cpp
* DESCRIPTION: file handling for TeXmacs
* COPYRIGHT  : (C) 1999  Joris van der Hoeven
*******************************************************************************
* This software falls under the GNU general public license version 3 or later.
* It comes WITHOUT ANY WARRANTY WHATSOEVER. For details, see the file LICENSE
* in the root directory or <http://www.gnu.org/licenses/gpl-3.0.html>.
******************************************************************************/

#include "tm_file.hpp"
#include "file.hpp"
#include "web_files.hpp"
#include "analyze.hpp"
#include "array.hpp"
#include "url.hpp"
#include "tmfs_url.hpp"
#include "scheme.hpp"
#include "convert.hpp"
#include "tm_timer.hpp"
#include "tree_label.hpp"
#include "tree_helper.hpp"
#include "sys_utils.hpp"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#ifndef OS_WIN
#include <sys/file.h>
#include <unistd.h>
#endif

#include <sys/types.h>
#include <string.h>  // strerror

#if defined (OS_MINGW) || defined (OS_WIN)
#include <time.h>
#include <dirent.h>
#else
#include <dirent.h>
#endif

#include "data_cache.hpp"

void system (string which, url u1) {
  lolly::system (which * " " * sys_concretize (u1)); }
void system (string which, url u1, url u2) {
  lolly::system (which * " " * sys_concretize (u1) * " " * sys_concretize (u2)); }
void system (string which, url u1, const char* post) {
  lolly::system (which * " " * sys_concretize (u1) * " " * post); }
void system (string which, url u1, const char* sep, url u2) {
  lolly::system (which * " " * sys_concretize (u1) * " " * sep *
	          " " * sys_concretize (u2)); }

/******************************************************************************
* New style loading and saving
******************************************************************************/

bool
load_string (url u, string& s, bool fatal) {
  // cout << "Load " << u << LF;
  url r= u;
  if (!is_rooted_name (r)) r= resolve (r);
  // cout << "Resolved " << r << LF;
  bool err= !is_rooted_name (r);
  if (!err) {
    string name= concretize (r);
    // cout << "Concrete :" << name << LF;
    // File contents in cache?
    bool file_flag= do_cache_file (name);
    bool doc_flag= do_cache_doc (name);
    string cache_type= doc_flag? string ("doc_cache"): string ("file_cache");
    if (doc_flag) cache_load ("doc_cache");
    bool currently_cached= is_cached (cache_type, name);
    if (currently_cached && is_up_to_date (url_parent (r))) {
      s= cache_get (cache_type, name) -> label;
      return false;
    }
    // End caching

    bench_start ("load file");
    c_string _name (name);
    // cout << "OPEN :" << _name << LF;
#ifdef OS_MINGW
    FILE* fin= fopen (_name, "rb");
#else
    FILE* fin= fopen (_name, "r");
    int fd= -1;
    if (fin != NULL) {
      fd= fileno (fin);
      if (flock (fd, LOCK_SH) == -1) {
        fclose (fin);
        fin= NULL;
      }
    }
#endif
    if (fin == NULL) {
      err= true;
      if (!occurs ("system", name))
        std_warning << "Load error for " << name << ", "
                    << strerror(errno) << "\n";
    }
    int size= 0;
    if (!err) {
      if (fseek (fin, 0L, SEEK_END) < 0) err= true;
      else {
        size= ftell (fin);
        if (size<0) err= true;
      }
      if (err) {
        std_warning << "Seek failed for " << as_string (u) << "\n";
#ifdef OS_MINGW
#else
        flock (fd, LOCK_UN);
#endif
        fclose (fin);
      }
    }
    if (!err) {
      rewind (fin);
      s->resize (size);
      int read= fread (&(s[0]), 1, size, fin);
      if (read < size) s->resize (read);
#ifdef OS_MINGW
#else
      flock (fd, LOCK_UN);
#endif
      fclose (fin);
    }
    bench_cumul ("load file");

    // Cache file contents
    if (!err && (N(s) <= 10000 || currently_cached))
      if (file_flag || doc_flag)
        cache_set (cache_type, name, s);
    // End caching
  }
  if (err) {
    string err_msg = string("Failed to load file: ") * as_string (u);
    if (fatal) {
      failed_error << err_msg << LF;
      TM_FAILED ("file not readable");
    }
    //else debug_io << err_msg << LF;
  }
  return err;
}

string string_load (url u) {
  string s;
  (void) load_string (u, s, false);
  return s;
}

bool
save_string (url u, string s, bool fatal) {
  if (is_rooted_tmfs (u)) {
    bool err= save_to_server (u, s);
    if (err && fatal) {
      failed_error << "File name= " << as_string (u) << "\n";
      TM_FAILED ("file not writeable");
    }
    return err;
  }

  // cout << "Save " << u << LF;
  url r= u;
  if (!is_rooted_name (r)) r= resolve (r, "");
  bool err= !is_rooted_name (r);
  if (!err) {
    string name= concretize (r);
    {
      c_string _name (name);
#ifdef OS_MINGW
      FILE* fout= fopen (_name, "wb");
#else
      FILE* fout= fopen (_name, "r+");
      bool rw= (fout != NULL);
      if (!rw) fout= fopen (_name, "w");
      int fd= -1;
      if (fout != NULL) {
        fd= fileno (fout);
        if (flock (fd, LOCK_EX) == -1) {
          fclose (fout);
          fout= NULL;
        }
        else if (rw) ftruncate (fd, 0);
      }
#endif
      if (fout == NULL) {
        err= true;
        std_warning << "Save error for " << name << ", "
                    << strerror(errno) << "\n";
      }
      if (!err) {
        int i, n= N(s);
        for (i=0; i<n; i++)
          fputc (s[i], fout);
#ifdef OS_MINGW
#else
        flock (fd, LOCK_UN);
#endif
        fclose (fout);
      }
    }
    // Cache file contents
    bool file_flag= do_cache_file (name);
    bool doc_flag= do_cache_doc (name);
    string cache_type= doc_flag? string ("doc_cache"): string ("file_cache");
    if (!err && N(s) <= 10000)
      if (file_flag || doc_flag)
        cache_set (cache_type, name, s);
    declare_out_of_date (url_parent (r));
    // End caching
  }

  if (err && fatal) {
    failed_error << "File name= " << as_string (u) << "\n";
    TM_FAILED ("file not writeable");
  }
  return err;
}

void string_save (string s, url u) {
  (void) save_string (u, s);
}

bool
append_string (url u, string s, bool fatal) {
  if (is_rooted_tmfs (u)) TM_FAILED ("file not appendable");

  // cout << "Save " << u << LF;
  url r= u;
  if (!is_rooted_name (r)) r= resolve (r, "");
  bool err= !is_rooted_name (r);
  if (!err) {
    string name= concretize (r);
    {
      c_string _name (name);
#ifdef OS_MINGW
      FILE* fout= fopen (_name, "ab");
#else
      FILE* fout= fopen (_name, "a");
      int fd= -1;
      if (fout != NULL) {
        fd= fileno (fout);
        if (flock (fd, LOCK_EX) == -1) {
          fclose (fout);
          fout= NULL;
        }
      }
#endif
      if (fout == NULL) {
        err= true;
        std_warning << "Append error for " << name << ", "
                    << strerror(errno) << "\n";
      }
      if (!err) {
        int i, n= N(s);
        for (i=0; i<n; i++)
          fputc (s[i], fout);
#ifdef OS_MINGW
#else
        flock (fd, LOCK_UN);
#endif
        fclose (fout);
      }
    }
    // Cache file contents
    declare_out_of_date (url_parent (r));
    // End caching
  }

  if (err && fatal) {
    failed_error << "File name= " << as_string (u) << "\n";
    TM_FAILED ("file not appendable");
  }
  return err;
}

void string_append_to_file (string s, url u) {
  (void) append_string (u, s);
}

void
append_to (url what, url to) {
  string what_s;
  if (load_string (what, what_s, false) ||
      append_string (to, what_s, false))
    std_warning << "Append failed for " << to << LF;
}


/******************************************************************************
* Getting attributes of a file
******************************************************************************/
url
url_numbered (url dir, string prefix, string postfix, int i) {
  if (!exists (dir)) mkdir (dir);
  for (; true; i++) {
    url name= dir * (prefix * as_string (i) * postfix);
    if (!exists (name)) return name;
  }
  return dir * (prefix * "x" * postfix);
}

url
url_scratch (string prefix, string postfix, int i) {
  url dir ("$TEXMACS_HOME_PATH/texts/scratch");
  return url_numbered (dir, prefix, postfix, i);
}

bool
is_scratch (url u) {
  return head (u) == url ("$TEXMACS_HOME_PATH/texts/scratch");
}

string
file_format (url u) {
  if (is_rooted_tmfs (u)) {
    return as_string (call ("tmfs-format", object (u)));
  }
  else return suffix_to_format (suffix (u));
}

/******************************************************************************
* Grepping of strings with heavy caching
******************************************************************************/

hashmap<tree,tree>   grep_cache (url_none () -> t);
hashmap<tree,string> grep_load_cache ("");
hashmap<tree,tree>   grep_complete_cache (url_none () -> t);

static bool
bad_url (url u) {
  if (is_atomic (u))
    return u == url ("aapi") || u == url (".svn");
  else if (is_concat (u))
    return bad_url (u[1]) || bad_url (u[2]);
  else return false;
}

string
grep_load (url u) {
  if (!grep_load_cache->contains (u->t)) {
    //cout << "Loading " << u << "\n";
    string s;
    if (load_string (u, s, false)) s= "";
    grep_load_cache (u->t)= s;
  }
  return grep_load_cache [u->t];
}

url
grep_sub (string what, url u) {
  if (is_or (u))
    return grep_sub (what, u[1]) | grep_sub (what, u[2]);
  else if (bad_url (u))
    return url_none ();
  else {
    string contents= grep_load (u);
    if (occurs (what, contents)) return u;
    else return url_none ();
  }
}

url
grep (string what, url u) {
  tree key= tuple (what, u->t);
  if (!grep_cache->contains (key)) {
    if (!grep_complete_cache->contains (u->t))
      grep_complete_cache (u->t)= expand (complete (u)) -> t;
    url found= grep_sub (what, as_url (grep_complete_cache [u->t]));
    grep_cache (key)= found->t;
  }
  return as_url (grep_cache [key]);
}

/******************************************************************************
* Finding recursive non hidden subdirectories of a given directory
******************************************************************************/

static void
search_sub_dirs (url& all, url root) {
  if (is_none (root));
  else if (is_or (root)) {
    search_sub_dirs (all, root[2]);
    search_sub_dirs (all, root[1]);
  }
  else if (is_directory (root)) {
    bool err= false;
    array<string> a= read_directory (root, err);
    if (!err) {
      for (int i=N(a)-1; i>=0; i--)
        if (N(a[i])>0 && a[i][0] != '.')
          search_sub_dirs (all, root * a[i]);
    }
    all= root | all;
  }
}

url
search_sub_dirs (url root) {
  url all= url_none ();
  //cout << "Search in " << root << " -> " << expand (complete (root, "dr")) << LF;
  search_sub_dirs (all, expand (complete (root, "dr")));
  return all;
}

/******************************************************************************
* Searching files in a directory tree with caching
******************************************************************************/

array<string> no_strings;
hashmap<tree,int> dir_stamp (0);
hashmap<tree,bool> dir_is_dir (false);
hashmap<tree,array<string> > dir_contents (no_strings);

array<string>
var_read_directory (url u) {
  array<string> d;
  if (is_rooted (u, "default") || is_rooted (u, "file")) {
    bool error_flag= false;
    array<string> a= read_directory (u, error_flag);
    for (int i=0; i<N(a); i++)
      if (!starts (a[i], "."))
        d << a[i];
  }
  return d;
}

url
search_file_in (url u, string name) {
  // cout << "Search in " << u << ", " << name << LF;
  if (!dir_stamp->contains (u->t) ||
      texmacs_time () - dir_stamp [u->t] > 10000) {
    dir_is_dir->reset (u->t);
    dir_contents->reset (u->t);
  }
  dir_stamp (u->t)= texmacs_time ();

  if (!dir_is_dir->contains (u->t))
    dir_is_dir (u->t)= is_directory (u);
  if (!dir_is_dir [u->t]) {
    if (as_string (tail (u)) == name) return u;
    return url_none ();
  }

  if (!dir_contents->contains (u->t)) {
    array<string> d= var_read_directory (u);
    dir_contents (u->t)= d;
  }

  array<string> d= dir_contents [u->t];
  for (int i=0; i<N(d); i++) {
    url f= search_file_in (u * d[i], name);
    if (!is_none (f)) return f;
  }
  return url_none ();
}

bool
find_stop (url u, array<string> stops) {
  if (head (u) == u) return false;
  for (int i=0; i<N(stops); i++)
    if (as_string (tail (u)) == stops[i]) return true;
  return find_stop (head (u), stops);
}


/******************************************************************************
* Searching text in the documentation
******************************************************************************/

static array<int>
search (string what, string in) {
  int i= 0, n= N(what);
  array<int> matches;
  if (n == 0) return matches;
  while (true) {
    int pos= search_forwards (what, i, in);
    if (pos == -1) return matches;
    matches << pos;
    i= pos+1;
  }
}

url
search_file_upwards (url u, string name, array<string> stops) {
  // cout << "Search upwards " << u << ", " << name << LF;
  url f= search_file_in (u, name);
  if (!is_none (f)) return f;
  if (head (u) == u) return url_none ();
  if (!find_stop (head (u), stops)) return url_none ();
  for (int i=0; i<N(stops); i++)
    if (as_string (tail (u)) == stops[i]) return url_none ();
  return search_file_upwards (head (u), name, stops);
}

static bool
precedes (string in, int pos, string what) {
  return pos >= N(what) && in (pos-N(what), pos) == what;
}

static int
compute_score (string what, string in, int pos, string suf) {
  int score= 1;
  if (pos > 0 && !is_iso_alpha (in [pos-1]))
    if (pos + N(what) + 1 < N(in) && !is_iso_alpha (in [pos+N(what)]))
      score *= 10;
  if (suf == "tm") {
    if (precedes (in, pos, "<")) score= 0;
    else if (precedes (in, pos, "<\\")) score= 0;
    else if (precedes (in, pos, "<|")) score= 0;
    else if (precedes (in, pos, "</")) score= 0;
    else if (precedes (in, pos, "compound|")) score= 0;
    else if (precedes (in, pos, "<name|")) score *= 10;
    else if (precedes (in, pos, "<tmstyle|")) score *= 10;
    else if (precedes (in, pos, "<tmdtd|")) score *= 10;
    else if (precedes (in, pos, "<explain-macro|")) score *= 10;
    else if (precedes (in, pos, "<var-val|")) score *= 10;
  }
  else if (suf == "scm") {
    if (precedes (in, pos, "define ")) score *= 10;
    else if (precedes (in, pos, "define-public ")) score *= 10;
    else if (precedes (in, pos, "define (")) score *= 10;
    else if (precedes (in, pos, "define-public (")) score *= 10;
    else if (precedes (in, pos, "define-macro ")) score *= 10;
    else if (precedes (in, pos, "define-public-macro ")) score *= 10;
    else if (precedes (in, pos, "define-macro (")) score *= 10;
    else if (precedes (in, pos, "define-public-macro (")) score *= 10;
  }
  return score;
}

static int
compute_score (string what, string in, array<int> pos, string suf) {
  int score= 0, i= 0, n= N(pos);
  for (i=0; i<n; i++)
    score += compute_score (what, in, pos[i], suf);
  return score;
}

string
escape_cork_words (string s) {
  int i;
  string r;
  for (i=0; i<N(s); i++) {
    if (s[i]=='<') {
      int j;
      for (j=i+1; j<N(s); j++)
        if (s[j]=='>') break;
      if (j<N(s)) j++;
      if (i+7==j && s[i+1]=='#' && s[j-1]=='>') {
        r << "\\<";
        r << s(i+1, j-1);
        r << "\\>";
        i=j-1;
      }
    } else {
      r << s[i];
    }
  }
  return r;
}

int
search_score (url u, array<string> a) {
  int n= N(a);
  string in= grep_load (u);
  if (N(in) == 0) return 0;

  string suf= suffix (u);
  if (suf == "tmml") {
    for (int i=0; i<n; i++)
      a[i]= cork_to_utf8 (a[i]);
  } else if (suf == "tm") {
    in= locase_all (in);
    for (int i=0; i<n; i++)
      a[i]= locase_all (escape_cork_words (a[i]));
  } else {
    in= locase_all (in);
    for (int i=0; i<n; i++)
      a[i]= locase_all (a[i]);
  }

  int score= 1;
  for (int i=0; i<n; i++) {
    string what= a[i];
    array<int> pos= search (what, in);
    score *= compute_score (what, in, pos, suf);
    if (score == 0) return 0;
    if (score > 1000000) score= 1000000;
  }
  return score;
}

/******************************************************************************
* Tab-completion for file names
******************************************************************************/

#ifdef OS_WIN
#define URL_CONCATER  '\\'
#else
#define URL_CONCATER  '/'
#endif

static void
file_completions_file (array<string>& a, url search, url u) {
  if (is_or (u)) {
    file_completions_file (a, search, u[1]);
    file_completions_file (a, search, u[2]);
  }
  else {
    url v= delta (search * url ("dummy"), u);
    if (is_none (v)) return;
    string s= as_string (v);
    if (is_directory (u)) s= s * string (URL_CONCATER);
    a << s;
  }
}

static void
file_completions_dir (array<string>& a, url search, url dir) {
  if (is_or (search)) {
    file_completions_dir (a, search[1], dir);
    file_completions_dir (a, search[2], dir);
  }
  else if (is_or (dir)) {
    file_completions_dir (a, search, dir[1]);
    file_completions_dir (a, search, dir[2]);
  }
  else {
    url u= search * dir * url_wildcard ("*");
    u= complete (u, "r");
    u= expand (u);
    file_completions_file (a, search, u);
  }
}

array<string>
file_completions (url search, url dir) {
  array<string> a;
  file_completions_dir (a, search, dir);
  return a;
}
