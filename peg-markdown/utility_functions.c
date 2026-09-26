/* utility_functions.c - List manipulation functions, element
 * constructors, and macro definitions for leg markdown parser. */

#include "utility_functions.h"
#include "markdown_peg.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>


/**********************************************************************

  List manipulation functions

 ***********************************************************************/

/* cons - cons an element onto a list, returning pointer to new head */
element * cons(element *new, element *list) {
    assert(new != NULL);
    new->next = list;
    return new;
}

/* reverse - reverse a list, returning pointer to new list */
element *reverse(element *list) {
    element *new = NULL;
    element *next = NULL;
    while (list != NULL) {
        next = list->next;
        new = cons(list, new);
        list = next;
    }
    return new;
}

/* concat_string_list - concatenates string contents of list of STR elements.
 * Frees STR elements as they are added to the concatenation. */
GString *concat_string_list(element *list) {
    GString *result;
    element *next;
    result = g_string_new("");
    while (list != NULL) {
        assert(list->key == STR);
        assert(list->contents.str != NULL);
        g_string_append(result, list->contents.str);
        next = list->next;
        free_element(list);
        list = next;
    }
    return result;
}

/**********************************************************************

  Global variables used in parsing

 ***********************************************************************/

char *charbuf = "";     /* Buffer of characters to be parsed. */
element *references = NULL;    /* List of link references found. */
element *notes = NULL;         /* List of footnotes found. */
element *parse_result;  /* Results of parse. */
int syntax_extensions;  /* Syntax extensions selected. */

/**********************************************************************

  Auxiliary functions for parsing actions.
  These make it easier to build up data structures (including lists)
  in the parsing actions.

 ***********************************************************************/

/* mk_element - generic constructor for element */
element * mk_element(int key) {
    element *result = malloc(sizeof(element));
    result->key = key;
    result->children = NULL;
    result->next = NULL;
    result->contents.str = NULL;
    return result;
}

/* mk_str - constructor for STR element */
element * mk_str(char *string) {
    element *result;
    assert(string != NULL);
    result = mk_element(STR);
    result->contents.str = strdup(string);
    return result;
}

/* mk_str_from_list - makes STR element by concatenating a
 * reversed list of strings, adding optional extra newline */
element * mk_str_from_list(element *list, bool extra_newline) {
    element *result;
    GString *c = concat_string_list(reverse(list));
    if (extra_newline)
        g_string_append(c, "\n");
    result = mk_element(STR);
    result->contents.str = c->str;
    g_string_free(c, false);
    return result;
}

/* mk_list - makes new list with key 'key' and children the reverse of 'lst'.
 * This is designed to be used with cons to build lists in a parser action.
 * The reversing is necessary because cons adds to the head of a list. */
element * mk_list(int key, element *lst) {
    element *result;
    result = mk_element(key);
    result->children = reverse(lst);
    return result;
}

/* mk_link - constructor for LINK element */
element * mk_link(element *label, char *url, char *title) {
    element *result;
    result = mk_element(LINK);
    result->contents.link = malloc(sizeof(link));
    result->contents.link->label = label;
    result->contents.link->url = strdup(url);
    result->contents.link->title = strdup(title);
    return result;
}

/* extension = returns true if extension is selected */
bool extension(int ext) {
    return (syntax_extensions & ext);
}

/* match_inlines - returns true if inline lists match (case-insensitive...) */
bool match_inlines(element *l1, element *l2) {
    while (l1 != NULL && l2 != NULL) {
        if (l1->key != l2->key)
            return false;
        switch (l1->key) {
        case SPACE:
        case LINEBREAK:
        case ELLIPSIS:
        case EMDASH:
        case ENDASH:
        case APOSTROPHE:
            break;
        case CODE:
        case STR:
        case HTML:
            if (strcasecmp(l1->contents.str, l2->contents.str) == 0)
                break;
            else
                return false;
        case EMPH:
        case STRONG:
        case LIST:
        case SINGLEQUOTED:
        case DOUBLEQUOTED:
            if (match_inlines(l1->children, l2->children))
                break;
            else
                return false;
        case LINK:
        case IMAGE:
            return false;  /* No links or images within links */
        default:
            fprintf(stderr, "match_inlines encountered unknown key = %d\n", l1->key);
            exit(EXIT_FAILURE);
            break;
        }
        l1 = l1->next;
        l2 = l2->next;
    }
    return (l1 == NULL && l2 == NULL);  /* return true if both lists exhausted */
}

/* find_reference - return true if link found in references matching label.
 * 'link' is modified with the matching url and title. */
bool find_reference(link *result, element *label) {
    element *cur = references;  /* pointer to walk up list of references */
    link *curitem;
    while (cur != NULL) {
        curitem = cur->contents.link;
        if (match_inlines(label, curitem->label)) {
            *result = *curitem;
            return true;
        }
        else
            cur = cur->next;
    }
    return false;
}

/* find_note - return true if note found in notes matching label.
if found, 'result' is set to point to matched note. */

bool find_note(element **result, char *label) {
   element *cur = notes;  /* pointer to walk up list of notes */
    while (cur != NULL) {
        if (strcmp(label, cur->contents.str) == 0) {
            *result = cur;
            return true;
        }
        else
            cur = cur->next;
    }
    return false;
}

/**********************************************************************

  GFM pipe table helpers

 ***********************************************************************/

/* text_has_unescaped_pipe - true if s contains a '|' not preceded by '\' */
bool text_has_unescaped_pipe(const char *s) {
    bool esc = false;

    while (*s) {
        if (esc) {
            esc = false;
        } else if (*s == '\\') {
            esc = true;
        } else if (*s == '|') {
            return true;
        }
        s++;
    }
    return false;
}

/* Split a table line on unescaped '|'. Decorative leading/trailing pipes
 * that produce empty edge cells are dropped; cells are trimmed. */
static GPtrArray *split_table_cells(const char *line) {
    GPtrArray *cells = g_ptr_array_new_with_free_func(g_free);
    GString *cur = g_string_new(NULL);
    bool esc = false;
    bool leading_pipe = (*line == '|');
    bool trailing_pipe = false;
    const char *p = line;
    const char *last_pipe = NULL;

    esc = false;
    for (const char *q = line; *q; q++) {
        if (esc) {
            esc = false;
        } else if (*q == '\\') {
            esc = true;
        } else if (*q == '|') {
            last_pipe = q;
        }
    }
    trailing_pipe = (last_pipe != NULL && last_pipe[1] == '\0');

    if (leading_pipe)
        p++;

    esc = false;
    while (*p) {
        if (esc) {
            g_string_append_c(cur, *p);
            esc = false;
        } else if (*p == '\\') {
            g_string_append_c(cur, *p);
            esc = true;
        } else if (*p == '|') {
            g_ptr_array_add(cells, g_string_free(cur, FALSE));
            cur = g_string_new(NULL);
        } else {
            g_string_append_c(cur, *p);
        }
        p++;
    }
    g_ptr_array_add(cells, g_string_free(cur, FALSE));

    if (trailing_pipe && cells->len > 0) {
        char *last_cell = cells->pdata[cells->len - 1];
        char *stripped = g_strstrip(last_cell);
        if (stripped[0] == '\0')
            g_ptr_array_remove_index(cells, cells->len - 1);
    }
    if (leading_pipe && cells->len > 0) {
        char *first_cell = cells->pdata[0];
        if (first_cell[0] == '\0')
            g_ptr_array_remove_index(cells, 0);
    }

    for (guint i = 0; i < cells->len; i++)
        g_strstrip(cells->pdata[i]);

    return cells;
}

static element *cells_to_list(GPtrArray *cells, int key) {
    element *row = mk_element(key);
    element *list = NULL;

    for (guint i = 0; i < cells->len; i++) {
        element *cell = mk_element(TABLECELL);
        cell->contents.str = strdup((char *) cells->pdata[i]);
        list = cons(cell, list);
    }
    row->children = reverse(list);
    return row;
}

/* parse_table_row_line - split a data/header line into a TABLEROW of cells.
 * Returns NULL only if the line has no unescaped pipe. */
element *parse_table_row_line(const char *line) {
    GPtrArray *cells;
    element *row;

    if (!text_has_unescaped_pipe(line))
        return NULL;
    cells = split_table_cells(line);
    row = cells_to_list(cells, TABLEROW);
    g_ptr_array_free(cells, TRUE);
    return row;
}

/* is_delim_cell - cell consists only of optional colons and >=1 dash */
static bool is_delim_cell(const char *cell) {
    const char *start = cell;
    const char *end = cell + strlen(cell);

    while (start < end && (*start == ' ' || *start == '\t'))
        start++;
    if (start < end && *start == ':')
        start++;
    {
        const char *dash = start;
        while (dash < end && *dash == '-')
            dash++;
        if (dash == start)
            return false;
        start = dash;
    }
    if (start < end && *start == ':')
        start++;
    while (start < end && (*start == ' ' || *start == '\t'))
        start++;
    return start == end;
}

static char delim_align(const char *cell) {
    const char *start = cell;
    const char *end = cell + strlen(cell);
    bool lead_colon = false, trail_colon = false;

    while (start < end && (*start == ' ' || *start == '\t'))
        start++;
    if (start < end && *start == ':') {
        lead_colon = true;
        start++;
    }
    while (end > start && (end[-1] == ' ' || end[-1] == '\t'))
        end--;
    if (end > start && end[-1] == ':')
        trail_colon = true;
    if (lead_colon && trail_colon)
        return 'c';
    if (trail_colon)
        return 'r';
    return 'l';
}

/* parse_table_delim_line - split delimiter row; each cell must be a delimiter */
element *parse_table_delim_line(const char *line) {
    GPtrArray *cells;
    element *row;

    if (!text_has_unescaped_pipe(line))
        return NULL;
    cells = split_table_cells(line);
    if (cells->len == 0) {
        g_ptr_array_free(cells, TRUE);
        return NULL;
    }
    for (guint i = 0; i < cells->len; i++) {
        if (!is_delim_cell((char *) cells->pdata[i])) {
            g_ptr_array_free(cells, TRUE);
            return NULL;
        }
    }
    row = cells_to_list(cells, TABLEROW);
    g_ptr_array_free(cells, TRUE);
    return row;
}

static int count_cells(element *row) {
    int n = 0;

    if (row == NULL)
        return 0;
    for (element *c = row->children; c != NULL; c = c->next)
        n++;
    return n;
}

/* table_validate - header/delimiter cell counts must match and be > 0.
 * Does not free either element; caller always owns them. */
bool table_validate(element *header, element *delim) {
    int hc = count_cells(header);
    int dc = count_cells(delim);

    if (hc > 0 && hc == dc) {
        header->key = TABLEHEADER;
        return true;
    }
    return false;
}

/* mk_table_element - assemble TABLE; rows_rev is a reversed cons list */
element *mk_table_element(element *header, element *delim, element *rows_rev) {
    element *table = mk_element(TABLE);
    GString *align;

    align = g_string_new("");
    for (element *c = delim->children; c != NULL; c = c->next) {
        if (c->contents.str)
            g_string_append_c(align, delim_align(c->contents.str));
    }
    table->contents.str = align->str;
    g_string_free(align, FALSE);

    table->children = cons(header, reverse(rows_rev));
    free_element_list(delim);
    return table;
}

/* Saved raw lines: pending_* updated during Table attempts (predicates run
 * immediately and may be overwritten by later failed attempts). On a fully
 * successful Table match, table_snapshot() pushes them onto a FIFO so each
 * deferred table_finish() gets the lines from its own match. */
static char *pending_header_line = NULL;
static char *pending_delim_line = NULL;
static GQueue *table_snapshots = NULL;

bool table_set_header(const char *line) {
    free(pending_header_line);
    pending_header_line = strdup(line ? line : "");
    return text_has_unescaped_pipe(pending_header_line);
}

bool table_set_delim(const char *line) {
    element *h;
    element *d;
    bool ok;

    free(pending_delim_line);
    pending_delim_line = strdup(line ? line : "");
    if (!text_has_unescaped_pipe(pending_delim_line))
        return false;

    h = parse_table_row_line(pending_header_line);
    d = parse_table_delim_line(pending_delim_line);
    ok = table_validate(h, d);
    free_element_list(h);
    free_element_list(d);
    return ok;
}

bool table_snapshot(void) {
    if (!table_snapshots)
        table_snapshots = g_queue_new();
    if (!pending_header_line || !pending_delim_line)
        return false;
    g_queue_push_tail(table_snapshots, strdup(pending_header_line));
    g_queue_push_tail(table_snapshots, strdup(pending_delim_line));
    return true;
}

element *table_finish(element *rows_rev) {
    element *header;
    element *delim;
    element *table;
    char *hdr_line;
    char *delim_line;

    if (!table_snapshots || g_queue_get_length(table_snapshots) < 2) {
        free_element_list(rows_rev);
        return mk_str("");
    }
    hdr_line = g_queue_pop_head(table_snapshots);
    delim_line = g_queue_pop_head(table_snapshots);

    header = parse_table_row_line(hdr_line);
    delim = parse_table_delim_line(delim_line);
    free(hdr_line);
    free(delim_line);
    if (header == NULL || delim == NULL || !table_validate(header, delim)) {
        free_element_list(header);
        free_element_list(delim);
        free_element_list(rows_rev);
        return mk_str("");
    }
    table = mk_table_element(header, delim, rows_rev);
    return table;
}

