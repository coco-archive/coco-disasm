/* ************************************************************ *
 * dasmedit.c - handles editing os9disam control files          $
 * first created Thu, 09 Sep 2004 18:19:02 -0500                $
 * $Id::                                                        $
 * ************************************************************ */


#include "g09dis.h"
#include <string.h>
#include <stdio.h>

GList *amode_list = NULL;

/* **************************************************************** *
 * List of standard addressing modes for insertion into the GList   *
 * for the gtkcombobox                                              *
 * **************************************************************** */

gchar *bounds_list[] = {"A - ASCII string",
                        "B - Byte data",
                        "W - Word data",
                        "S - Short Label data",
                        "L - Long Label data",
                         NULL};

/*  -------------------  generic functions first ------------------------ */


/* ******************************************************** *
 * doc_set_modified():                                      *
 *                                                          *
 *     sets the FILEINF->altered flag to value              *
 *     if it is already this value, do nothing              *
 * ******************************************************** */

void
doc_set_modified(FILEINF *doc, gboolean value)
{
    if(doc->altered != value)
    {
        doc->altered = value;
    }
}

/* **************************************************************** *
 * add_combo_class () : GFunc callback to add entries to combobox   *
 *      from GList                                                  *
 * **************************************************************** */

static void
add_combo_class (G_CONST_RETURN gchar *lclass, GtkWidget **cbox)
{
    gtk_combo_box_append_text (GTK_COMBO_BOX(*cbox), lclass);
}

/* **************************************************************** *
 *   bounds_aligned_frame () - create a frame with alignment        *
 *   Passed: box = box to contain frame title = label for frame     *
 *   Returns: alignment in frame (the outtermost widget)            *
 * **************************************************************** */

GtkWidget *
bounds_aligned_frame (GtkBox *box, gchar *title)
{
    GtkWidget *frame,
              *alignment;
    
    frame = gtk_frame_new(title);
    /*gtk_frame_set_shadow_type (GTK_FRAME(frame), GTK_SHADOW_IN);*/
    alignment = gtk_alignment_new ( .5, .5, 0, 0);
    gtk_alignment_set_padding (GTK_ALIGNMENT(alignment), 15, 15,15,15);
    gtk_container_add (GTK_CONTAINER(frame),alignment);
    gtk_box_pack_start(box, frame, FALSE, FALSE, 5);

    return alignment;
}

/* ******************************************************************** *
 * build_label_selector() - build a combobox and include the selections *
 * PASSED: modept - if NULL, build addressing modes from GList          *
 *         amode_list, else build label class from gchar **             *
 * Returns: the combo_box_entry                                         *
 * ******************************************************************** */

GtkWidget *
build_label_selector (gchar **modept, gboolean with_entry)
{
    GtkWidget *combo_box;

    if (with_entry)
    {
        combo_box = gtk_combo_box_entry_new_text();
    }
    else {
        combo_box = gtk_combo_box_new_text();
    }
    
    if (modept)
    {       /* We're building addressing mode from gchar array */
        
        while (*modept) {
            gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box) , *modept);
            ++modept;
        };
    }
    else {
        if ( ! amode_list)    /* never been initialized? */
        {
            amode_init();
        }

        g_list_foreach (amode_list, (GFunc)add_combo_class,
                        &(combo_box));
    }
    
    return combo_box;
    
}

/* ----------------- end general functions begin locals ----------------- */

/* ************************************************************************ *
 * txtbuf_insert_line() - Inserts text on a new line after the line where   *
 *      the cursor is.  It automatically prepends a newline to the string,  *
 *      moves the cursor to the end of the current line, and inserts the    *
 *      text.                                                               *
 * Passed : (1) the GtkTextBuffer into which to insert the text.            *
 *          (2) a GString containing a NULL-terminated string (no newline)  *
 * ************************************************************************ */

void
txtbuf_insert_line (GtkTextBuffer *t_buf, GString *g_str)
{
    GtkTextMark *mark;
    GtkTextIter iter;

    mark = gtk_text_buffer_get_insert (O9Dis.cmdfile.tbuf);
    gtk_text_buffer_get_iter_at_mark (O9Dis.cmdfile.tbuf, &iter, mark);

    if ( gtk_text_iter_starts_line (&iter))
    {
        g_string_append (g_str, "\n");
    }
    else
    {
        if ( ! gtk_text_iter_ends_line (&iter))
        {
            gtk_text_iter_forward_to_line_end (&iter);
            gtk_text_buffer_place_cursor (t_buf, &iter);
        }

        g_string_prepend (g_str, "\n");
    }

    gtk_text_buffer_insert_at_cursor (t_buf, g_str->str, -1);
}

static void
on_bnds_define_response (GtkDialog *dialog, gint resp,
                         struct adr_widgets *data)
{
    switch (resp) {
        gint indx;
        GString *line;
        
        case GTK_RESPONSE_OK:
            line = g_string_new(NULL);

            if ((indx = gtk_combo_box_get_active(GTK_COMBO_BOX(
                                                 data->label_combo))) >= 0)
            {
                g_string_append_printf(line, "%c ", *bounds_list[indx]);
            }

            if( (*(line->str) == 'S') || (*(line->str) == 'L'))
            {
                gchar *offsetdef;

                g_string_append_printf(line, "%c ",
                                       *(gtk_entry_get_text(GTK_ENTRY (
                                            GTK_BIN(
                                                data->label_entry)->child))));
#if (GTK_MAJOR_VERSION < 3) && (GTK_MINOR_VERSION < 6)
                offsetdef = gtk_entry_get_text (GTK_ENTRY (GTK_BIN (
                                        data->offset_entry)->child));
#else
                offsetdef = gtk_combo_box_get_active_text (
                                        GTK_COMBO_BOX(data->offset_entry));
#endif

                if ( strcmp (offsetdef, "NONE"))
                {
                    g_string_append_printf (line, "(%s) ", offsetdef);
                }
            }
            
            g_string_append(line, gtk_entry_get_text(GTK_ENTRY(
                                                     data->address_entry)));
            txtbuf_insert_line (O9Dis.cmdfile.tbuf, line);

//            doc_set_modified(&O9Dis.cmdfile, TRUE);
            
            g_string_free(line, TRUE);
            break;
        default:
            break;
    }
    /*g_free (data);*/ /* We now keep it permanently after creating */
    gtk_widget_hide (GTK_WIDGET(dialog));
}

/* ******************************************************************** *
 * abort_warn() - pop up a dialog notifying that a function cannot      *
 *                continue                                              *
 * ******************************************************************** */

void abort_warn (char *msg)
{
    GtkWidget *dialog;
    gchar *warnmsg;

    warnmsg = g_strconcat(msg, "\n\n", "Cannot continue with task", NULL);
    
    dialog = gtk_message_dialog_new (GTK_WINDOW(w_main),
                                     GTK_DIALOG_DESTROY_WITH_PARENT |
                                     GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_WARNING,
                                     GTK_BUTTONS_OK,
                                     warnmsg);
    gtk_dialog_run (GTK_DIALOG (dialog));
    g_free (warnmsg);
    gtk_widget_destroy (dialog);
}

/* **************************************************************** *
 * build_dialog_cancel_save() - create a dialog with a Cancel and a *
 *          custom "Save" button.                                   *
 * Passed: char * to title for top                                  *
 * Returns: GtkWidget * for the new dialog                          *
 * **************************************************************** */

GtkWidget *
build_dialog_cancel_save (gchar *title, gboolean hide_on_delete)
{
    GtkWidget *dlg;
    dlg = gtk_dialog_new_with_buttons(title,
                                      GTK_WINDOW(w_main),
                                      /*GTK_DIALOG_MODAL |*/
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_STOCK_CANCEL,
                                      GTK_RESPONSE_REJECT,
                                      "Save",
                                      GTK_RESPONSE_OK,
                                      NULL);

    gtk_container_set_border_width(GTK_CONTAINER(dlg), 15);

    if (hide_on_delete)
    {
        g_signal_connect (dlg, "delete-event",
                               G_CALLBACK(gtk_widget_hide_on_delete), NULL);
    }

    return dlg;
}

/* **************************************************************** *
 * sensitize_amode() - Callback for when Boundary Type combo is     *
 *      changed.  Sensitizes amode if mode is changed to Short or   *
 *      Long label type, desititizes it otherwise.                  *
 * **************************************************************** */

void
sensitize_amode (GtkComboBox *btype, struct adr_widgets *wdgs)
{
    GtkTreeIter iter;
    gchar *txt;
    gboolean sen_state;

    gtk_combo_box_get_active_iter (btype, &iter);
    gtk_tree_model_get (gtk_combo_box_get_model (btype), &iter, 0, &txt, -1);

    switch (*txt)
    {
        case 'S':
        case 'L':
            sen_state = TRUE;
            break;
        default:
            sen_state = FALSE;
    }
            gtk_widget_set_sensitive (wdgs->label_entry, sen_state);
            gtk_widget_set_sensitive (wdgs->offset_entry, sen_state);
}

/* ******************************************************************** *
 * bnds_define_cb() - Callback function to handle boundary definitions  *
 *      The dialog and widget list are created upon first call to       *
 *      function and then kept for the duration of program.             *
 * ******************************************************************** */

void
bnds_define_cb (GtkAction *action, glbls *fdat)
{
    static GtkWidget *bounds_dlg;
    static struct adr_widgets *bnds_widgets;

    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(
                                                 (fdat->list_file).tview));
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        GtkWidget *align;
        
        gchar *addr;
       
        if ( ! bnds_widgets)
        {
            if (!(bnds_widgets = g_malloc (sizeof (struct adr_widgets))))
            {
                /* Error report */
                abort_warn ("Error! cannot malloc memory!\n");
                return;
            }
        }
        
        if ( ! bounds_dlg)
        {
            bounds_dlg = build_dialog_cancel_save ("Define Data Bounds", TRUE);
          
            g_signal_connect (bounds_dlg, "response",
                          G_CALLBACK(on_bnds_define_response), bnds_widgets);

            /* ******************************** *
             *   Boundary Type dropdown menu    *
             * ******************************** */
            
            align = bounds_aligned_frame (GTK_BOX(GTK_DIALOG(bounds_dlg)->vbox),
                                          "Boundary type");
            bnds_widgets->label_combo
                = build_label_selector (bounds_list, FALSE);
            gtk_container_add (GTK_CONTAINER(align),bnds_widgets->label_combo);

            /* ******************************** *
             *   Addressing Mode dropdown menu  *
             * ******************************** */
            
            align = bounds_aligned_frame (GTK_BOX(GTK_DIALOG(bounds_dlg)->vbox),
                                          "Addressing Mode");
            bnds_widgets->label_entry = build_label_selector (NULL, TRUE);
            gtk_widget_set_sensitive (bnds_widgets->label_entry, FALSE);
            gtk_container_add (GTK_CONTAINER(align),bnds_widgets->label_entry);

            g_signal_connect (bnds_widgets->label_combo, "changed",
                              G_CALLBACK(sensitize_amode), bnds_widgets);

            /* ******************************** *
             *   Address dropdown menu          *
             * ******************************** */
            
            align = bounds_aligned_frame (GTK_BOX(GTK_DIALOG(bounds_dlg)->vbox),
                                          "Address or Address Range");
            bnds_widgets->address_entry = gtk_entry_new();
            gtk_container_add (GTK_CONTAINER(align),
                               bnds_widgets->address_entry);

            /* ******************************** *
             *   Offset dropdown menu           *
             * ******************************** */
            
            bnds_widgets->offset_entry =
                pack_offset_entry (GTK_BOX(GTK_DIALOG(bounds_dlg)->vbox));
          
            gtk_widget_show_all(GTK_WIDGET(GTK_DIALOG(bounds_dlg)->vbox));
        }

        gtk_combo_box_set_active (GTK_COMBO_BOX(bnds_widgets->label_combo), 0);
        gtk_combo_box_set_active (GTK_COMBO_BOX(bnds_widgets->label_entry), 0);

        gtk_tree_model_get (model, &iter, LST_ADR, &addr, -1);
        gtk_entry_set_text (GTK_ENTRY(bnds_widgets->address_entry), addr);
        g_free (addr);

        gtk_entry_set_text (
                    GTK_ENTRY (GTK_BIN(bnds_widgets->offset_entry)->child),
                    "NONE");
        gtk_widget_set_sensitive (bnds_widgets->offset_entry, FALSE);
        gtk_widget_show (bounds_dlg);
    }
}

/* callback function for exit from label definition */

static void
name_label_response (GtkDialog *dialog, gint resp,
                     struct adr_widgets *data)
{
    GtkTreeIter iter;
    gchar *class = g_strndup (gtk_entry_get_text(GTK_ENTRY(GTK_BIN(
                                                 data->label_combo)->child)),
                              1);
    gchar *hexaddr = NULL;
    
    hexaddr = g_strconcat ("$", gtk_entry_get_text(GTK_ENTRY(
                                                       data->address_entry)),
                                                       NULL);
    
    switch (resp) {
        case GTK_RESPONSE_OK:
            gtk_list_store_append(O9Dis.lblfile.l_store, &iter);
            gtk_list_store_set (O9Dis.lblfile.l_store, &iter,
                                LBL_LBL, gtk_entry_get_text(GTK_ENTRY(
                                                    data->label_entry)),
                                LBL_EQU, "equ",
                                LBL_ADDR, hexaddr,
                                LBL_CLASS, class,
                                -1);
            doc_set_modified(&O9Dis.lblfile, TRUE);
            
            g_free(class); g_free(hexaddr);
            break;
        default:
            break;
    }

    /*g_free (data);*/ /* We now keep the widget list */
    gtk_widget_hide(GTK_WIDGET(dialog));
}

/* ******************************************** *
 * callback function to handle label name mode  *
 * ******************************************** */

void
rename_label (GtkAction * action, glbls *fdat)
{
    static struct adr_widgets *cb_data;
    static GtkWidget *dialog;

    GtkWidget *align;
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection *selection;


    selection = gtk_tree_view_get_selection (GTK_TREE_VIEW(
                                                 (fdat->list_file).tview));
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        gchar *addr, *lblname, *opcod, *pbyt, *mnem, *oprand;

        char adr_mode[4];
       
        if ( ! cb_data)
        {
            if (!(cb_data = g_malloc (sizeof (struct adr_widgets))))
            {
                /*report error */
                abort_warn ("Error! cannot allocate memory for struct\n");
                return;
            }
        }

        gtk_tree_model_get (model, &iter, LST_ADR, &addr,
                                          LST_LBL, &lblname,
                                          LST_OPCO, &opcod,
                                          LST_PBYT, &pbyt,
                                          LST_MNEM, &mnem,
                                          LST_OPER, &oprand,
                                          -1);

        /* *************************
         * Get addressing mode for current command
         * ************************* */
        
        if ( !(cb_data->cmd_mode =
                    get_addressing_mode(adr_mode, opcod, mnem, oprand)) )
        {
            cb_data->cmd_mode = "L";
        }
    
        /* ******************** *
         * Label name entry box *
         * ******************** */

        if ( ! dialog)
        {
            dialog = build_dialog_cancel_save ("Define Label Name", TRUE);
            /* The following assignment is needless,
             * but it keeps gcc quiet about
             * it "might be used uninitialized"
             *
            cb_data->cmd_mode = NULL;*/
           
            g_signal_connect(dialog, "response",
                    G_CALLBACK(name_label_response), cb_data);
        

            align = bounds_aligned_frame (GTK_BOX(GTK_DIALOG(dialog)->vbox),
                                          "Label Name");
            cb_data->label_entry = gtk_entry_new();
            gtk_container_add (GTK_CONTAINER(align), cb_data->label_entry);
                    
            /* ***************** *
             * Address entry box *
             * ***************** */
        
            align = bounds_aligned_frame (GTK_BOX(GTK_DIALOG(dialog)->vbox),
                                          "Label Address");
            cb_data->address_entry = gtk_entry_new();
            gtk_container_add (GTK_CONTAINER(align), cb_data->address_entry);
            

            /* ******************************************* *
             * Build selection for desired Addressing Mode *
             * ******************************************* */
            
            align = bounds_aligned_frame (GTK_BOX(GTK_DIALOG(dialog)->vbox),
                                          "Addressing Mode");
            
            cb_data->label_combo = build_label_selector (NULL, TRUE);
            gtk_container_add (GTK_CONTAINER(align), cb_data->label_combo);

            gtk_widget_show_all (GTK_WIDGET(GTK_DIALOG(dialog)->vbox));
        }
        /*gtk_dialog_run(GTK_DIALOG(dialog));*/

        if( strlen(lblname) )
        {
            gtk_entry_set_text (GTK_ENTRY(cb_data->label_entry), lblname);
        }

        gtk_entry_set_text(GTK_ENTRY(GTK_BIN(cb_data->label_combo)->child),
                               guess_addr_mode(mnem));
        gtk_entry_set_text (GTK_ENTRY(cb_data->address_entry), addr);
        g_free(addr); g_free(opcod); g_free(pbyt);
        g_free(oprand); g_free(mnem); g_free(lblname);
        
        gtk_widget_show (dialog);
    }

}

/* **************************************************** *
 * lbl_edit_line() : create dialog for editing/adding   *
 *          line (row) to the Label tree_store          *
 * Returns: button respons from dialog                  *
 * **************************************************** */

static gint
lbl_edit_line(gchar **label, gchar **addr, gchar **class)
{
    GtkWidget *dialog,
              *align,
              *name_ent, *addr_ent, *class_ent;
    gint response;
   
    dialog = build_dialog_cancel_save ("Edit Label Line", FALSE);

    align = bounds_aligned_frame (GTK_BOX(GTK_DIALOG(dialog)->vbox),
                                  "Label Name");
    name_ent = gtk_entry_new();
    gtk_container_add (GTK_CONTAINER(align), name_ent);

    if( *label )
    {
        gtk_entry_set_text (GTK_ENTRY(name_ent), *label);
    }
    
    align = bounds_aligned_frame (GTK_BOX(GTK_DIALOG(dialog)->vbox),
                                  "Label Address");
    addr_ent = gtk_entry_new();
    gtk_container_add (GTK_CONTAINER(align), addr_ent);

    if(*addr)
    {
        gtk_entry_set_text (GTK_ENTRY(addr_ent), *addr);
    }
    
    align = bounds_aligned_frame (GTK_BOX(GTK_DIALOG(dialog)->vbox),
                                  "Label Class");
    /*class_ent = gtk_entry_new();*/
    class_ent = build_label_selector(NULL, TRUE);
    gtk_container_add (GTK_CONTAINER(align), class_ent);

    if(*class)
    {
        gtk_entry_set_text (GTK_ENTRY (GTK_BIN(class_ent)->child), *class);
    }
    
    gtk_widget_show_all (GTK_WIDGET (GTK_DIALOG(dialog)->vbox));

    response = gtk_dialog_run (GTK_DIALOG (dialog));
    
    *label = g_strdup (gtk_entry_get_text (GTK_ENTRY(name_ent)));
    *addr = g_strdup (gtk_entry_get_text (GTK_ENTRY(addr_ent)));
    *class = g_strndup (gtk_entry_get_text(GTK_ENTRY (GTK_BIN (
                                                   class_ent)->child)), 1);
    
    gtk_widget_destroy(dialog);
    
    return response;
}

/* ************************************************************ *
 * lbl_insert_line() : insert a  line in the label entry list   *
 * ************************************************************ */

void
lbl_insert_line (GtkAction * action, glbls *fdat)
{
    GtkTreeModel *model;
    GtkTreeIter iter, blank_iter, *new_iter;
    GtkTreeSelection *selection;

    gchar *label = NULL,
          *addr = NULL,
          *class = NULL;
    gint response;
    
    while(1) {
        response = lbl_edit_line(&label, &addr, &class);
        if ((response != GTK_RESPONSE_OK))
        {
            return; /* cancelled */
        }

        if( strlen(label) && strlen(addr) && strlen(class) )
        {
            /* add entry to Label tree_store */
            break;
        }
        else {
            GtkWidget *err_dial;

            /* warn about error and redo dialog */
            err_dial = gtk_message_dialog_new (GTK_WINDOW(w_main),
                                               GTK_DIALOG_DESTROY_WITH_PARENT |
                                                  GTK_DIALOG_MODAL,
                                               GTK_MESSAGE_WARNING,
                                               GTK_BUTTONS_OK,
                                               "An element of the line is not set\nPlease redo");
            gtk_dialog_run(GTK_DIALOG(err_dial));
            gtk_widget_destroy(err_dial);
        }
    }
    
    selection = 
        gtk_tree_view_get_selection (GTK_TREE_VIEW(O9Dis.lblfile.tview));
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        gtk_list_store_insert_after (GTK_LIST_STORE(model),
                                     &blank_iter, &iter);
        new_iter = &blank_iter;
    }
    else {
        gtk_list_store_append (GTK_LIST_STORE(model), &iter);
        new_iter = &iter;
    }
    
    /*label = gtk_entry_get_text(GTK_ENTRY(name_ent));
    addr = gtk_entry_get_text(GTK_ENTRY(addr_ent));
    class = gtk_entry_get_text(GTK_ENTRY(class_ent));*/

    gtk_list_store_set (GTK_LIST_STORE(model), new_iter,
                                      LBL_LBL, label,
                                      LBL_EQU, "equ",
                                      LBL_ADDR, addr,
                                      LBL_CLASS, class,
                                      -1);
    g_free(label); g_free(addr); g_free(class);
    
    doc_set_modified(&O9Dis.lblfile, TRUE);
}
        
/* ************************************************************ *
 * lbl_delete_line() : delete a line in the label entry list    *
 * ************************************************************ */

void
lbl_delete_line (GtkAction * action, glbls *fdat)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    
    selection = 
        gtk_tree_view_get_selection (GTK_TREE_VIEW(O9Dis.lblfile.tview));
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        gtk_list_store_remove (GTK_LIST_STORE(model), &iter);
    }
}

/* ************************************************ *
 * lbl_properties() : edit an existing label entry  *
 * ************************************************ */

void
lbl_properties (GtkAction * action, glbls *fdat)
{
    GtkTreeModel *model;
    GtkTreeIter iter;
    GtkTreeSelection *selection;
    
    selection = 
        gtk_tree_view_get_selection (GTK_TREE_VIEW(O9Dis.lblfile.tview));
    
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        G_CONST_RETURN gchar *t_label,
                             *t_addr,
                             *t_class;
        gchar *label,
              *addr,
              *class;

        gtk_tree_model_get (model, &iter, LBL_LBL, &t_label,
                                          LBL_ADDR, &t_addr,
                                          LBL_CLASS, &t_class,
                                          -1);

        label = g_strdup (t_label);
        addr = g_strdup (t_addr);
        class = g_strdup (t_class);

        if (lbl_edit_line (&label, &addr, &class) == GTK_RESPONSE_OK )
        {
            /*label = gtk_entry_get_text(GTK_ENTRY(name_ent));
            addr = gtk_entry_get_text(GTK_ENTRY(addr_ent));
            class = gtk_entry_get_text(GTK_ENTRY(class_ent));*/

            gtk_list_store_set (GTK_LIST_STORE(model), &iter,
                                              LBL_LBL, label,
                                              LBL_ADDR, addr,
                                              LBL_CLASS, class,
                                              -1);
            g_free (label); g_free (addr); g_free (class);

            doc_set_modified (&O9Dis.lblfile, TRUE);
        }
        
    }
}
