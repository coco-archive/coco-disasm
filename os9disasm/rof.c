
/* ************************************************** *
 * rof.c - handles rof files                          *
 *                                                    *
 * $Id$                                               *
 * ************************************************** */

#include "odis.h"

/* symbol table types */
/* symbol definition/reference type/location */
/* location flags */
#define RELATIVE    0x80        /* Store relative to location of reference */
#define NEGMASK     0x40        /* negate on resolution */
#define CODLOC      0x20        /* Reference is in CODE */
#define DIRLOC      0x10        /* if data, store in DP */
/* type flags */
#define LOC1BYT     0x08        /* one byte long */
/* What reference refers to */
#define CODENT      0x04        /* refers to CODE */
#define DIRENT      0x02        /* refers to DP data */
#define INIENT      0x01        /* refers to init data */
#define LOCMASK     (CODLOC|DIRLOC)

struct rof_glbl *glbl_code = 0,
                *glbl_dp = 0,
                *glbl_nondp = 0,
                *glbls;                     /* Generic global pointer */
/*struct rof_extrn *xtrn_ndp[] = {0, 0},
                 *xtrn_dp[] = {0, 0},*/
struct rof_extrn *xtrn_ndp = 0,
                 *xtrn_dp = 0,
                 *xtrn_code = 0,
                 *extrns;                   /* Generic external pointer */
struct nlist *dp_base,
             *vsect_base,
             *code_base,
             *lblptr;

struct rof_hdr ROF_hd,
               *rofptr = &ROF_hd;

int code_begin,
    idp_begin,
    indp_begin;

extern int CodEnd,
           PrevEnt;
extern char *realcmd;
extern struct printbuf *pbuf;

/* ************************************************** *
 * fstrncpy() - equivalent to strncpy, except that    *
 *           the source comes from a stdio filehandle *
 *           Will copy n-1 chars, and returns a null- *
 *           terminated string                        *
 * Passed: char *dest - the destination               *
 *         int  n     - the maximum number of chars   *
 *         FILE *fp   - the file ptr for the source   *
 * Returns: pointer to the stored string              *
 * ************************************************** */

char *
fstrncpy (char *dest, int n, FILE *fp)
{
    char *pt = dest;

    while (n-- )
    {
        if (!(*(pt++) = fgetc (fp)))    /* Null == end of string */
        {
            return dest;
        }
    }

    /* If we get here, we've maxed out the count */
    
    *pt = '\0';
    return dest;
}

/* ************************************************** *
 * rofhdr() - read and interpret rof header           *
 * ************************************************** */

void
rofhdr (void)
{
    int glbl_cnt,
        ext_count,
        count;          /* Generic counter */

    /* get header data */

    rofptr->sync[0] = o9_fgetword (progpath);
    rofptr->sync[1] = o9_fgetword (progpath);
    rofptr->ty_lan = o9_fgetword (progpath);
    rofptr->valid = fgetc (progpath);
    
    if ((count = fread (rofptr->rdate, sizeof (rofptr->rdate), 1, progpath)) != 1)
    {
        fprintf (stderr, "read() FAILED for date - read %d bytes\n",count);
    }

    rofptr->edition = fgetc (progpath);
    rofptr->version = fgetc (progpath);
    rofptr->udatsz = o9_fgetword (progpath);
    rofptr->udpsz = o9_fgetword (progpath);
    rofptr->idatsz = o9_fgetword (progpath);
    rofptr->idpsz = o9_fgetword (progpath);
    rofptr->codsz = o9_fgetword (progpath);
    rofptr->stksz = o9_fgetword (progpath);
    rofptr->modent = o9_fgetword (progpath);
    fstrncpy (rofptr->rname, sizeof (rofptr->rname), progpath);

    /* Set ModData to an unreasonable high number so ListData
     * won't do it's thing...
     */

    ModData = 0x7fff;

    count = glbl_cnt = o9_fgetword (progpath);

    while (count--) {
        char name[10];
        struct nlist *me;
        int adrs;
        int typ;

/*        if (!(lblptr = calloc (1, sizeof(struct nlist)))) {
            errexit ("Cannot allocate memory for rof_glbl");
        }

        if (!(glbls = calloc (1, sizeof (struct (rof_glbl)))))
        {
            nerrexit ("Cannot allocate memory for rof_glbl");
        }*/

        fstrncpy (name, sizeof (glbls->name), progpath);
        typ = fgetc (progpath);
        adrs = o9_fgetword (progpath);

        if ((me = addlbl (adrs, rof_class (typ)))) {
            strcpy (me->sname, name);
            me->global = 1;
        }
    }

    /* Code section... read, or save file position   */
    code_begin = ftell (progpath);
    idp_begin = code_begin + rofptr->codsz;
    indp_begin = idp_begin + rofptr->idpsz;

    fseek (progpath, rofptr->codsz + rofptr->idatsz + rofptr->idpsz, SEEK_CUR);

    /* external references... */

    ext_count = o9_fgetword (progpath);

    while (ext_count--) {
        char _name[10];
        int ncount;

        fstrncpy (_name, sizeof (_name), progpath);
        ncount = o9_fgetword (progpath);

        /* Get the individual occurrences for this name */

        get_refs (_name, ncount, 1);
    }
    
    /* Local variables... */

    ext_count = o9_fgetword (progpath);

    get_refs("", ext_count, 0);

    /* common block variables... */
    /* Do this after everything else is done */

    /* Now we're ready to disassemble the code */

    CodEnd = rofptr->codsz;

    fseek (progpath, code_begin, SEEK_SET);

   /* rofdis();*/
}

/* ****************************************************** *
 * rof_class () - returns the Destination reference for   *
 *          the reference                                 *
 * Passed: The Type byte from the reference               *
 * Returns: The Class Letter for the entry                *
 * ****************************************************** */

char
rof_class (int typ)
{
    int class;

    /* We'll tie up additional classes for data/bss as follows
     * C for uninit non-dp
     * D for uninit DP
     * E for init non-dp
     * F for init DP
     *
     */

    char klasses[5] = "CDLEF",
         *real_to = klasses;

    if (typ & 1)
    {
        real_to += 3;
    }

    class = (typ & 0x07) >> 1;

    return real_to[class];
}

/* ************************************************** *
 * rof_addlbl() - Adds a label to the nlist tree if   *
 *                applicable                          *
 *                Copies rof name to nlist name if    *
 *                different
 * Passed: adrs - address of label                    *
 *         ref  - reference structure                 *
 * ************************************************** */

void
rof_addlbl (int adrs, struct rof_extrn *ref)
{
    struct nlist *nl;

    if ((nl = addlbl (adrs, rof_class (ref->Type))))
    {
        if (strlen (ref->name))
        {
            if (strcmp (nl->sname, ref->name))
            {
                strcpy (nl->sname, ref->name);
            }
        }
    }
}

/* ************************************************** *
 * get_refs() - get entries for given reference,      *
 *    either external or local.                       *
 * Passed:  name (or blank for locals)                *
 *          count - number of entries to process      *
 *          1 if external, 0 if local                 *
 * ************************************************** */

void get_refs(char *vname, int count, int is_extern)
{
    while (count--) {
        unsigned char _ty;
        int _ofst;
        struct rof_extrn *me,
                         **base = 0;

        _ty = fgetc (progpath);
        _ofst = o9_fgetword (progpath);

        /* Add to externs table */

        switch (_ty & 0xf0)
        {
            case 0x00:
                /*base = &(xtrn_ndp[_ty & 1]);*/
                base = &xtrn_ndp;
                extrns = *base;
                break;
            case 0x10:
                /*base = &(xtrn_dp[_ty & 1]);*/
                base = &xtrn_dp;
                extrns = *base;
                break;

                /* Try to do both pcr and non-pcr together
                 * Don't think it will matter here
                 */
            case 0x20:
            case 0xa0:
                base = &xtrn_code;
                extrns = xtrn_code;
                break;
            default:        /* Error */
                nerrexit ("Unknown external type encountered");
        }

        if ( !(me=calloc(1, sizeof(struct rof_extrn))))
        {
            nerrexit ("Cannot allocate memory for rof external ref");
        }

        if (strlen(vname))
        {
            strncpy (me->name, vname, sizeof(me->name));
        }
        
        me->Type = _ty;
        me->Ofst = _ofst;
        me->Extrn = is_extern;
    
        /* If this tree has not yet been initialized, simply set the
         * base pointer to this entry (as the first)
         */
    
        if (!*base)
        {
            *base = me;
        }
    
        /* If we get here, this particular tree has alreay been started,
         * so find where to put the new entry.  Note, for starters, let's
         * assume that each entry will be unique, that is, this location
         * won't be here
         */
    
        else
        {
            extrns = *base;     /* Use the global externs pointer */
        
            while (1) {
                struct rof_extrn **to_me = 0;
        
                if (_ofst < extrns->Ofst)
                {
                    if (extrns->LNext) {
                        extrns = extrns->LNext;
                        continue;
                    }
                    else {
                        to_me = &(extrns->LNext);
                    }
                }
                else {
                    if (_ofst > extrns->Ofst)
                    {
                        if (extrns->RNext) {
                            extrns = extrns->RNext;
                            continue;
                        }
                        else {
                            to_me = &(extrns->RNext);
                        }
                    }
                }
        
                if (to_me)          /* I.E. found the proper empty spot */
                {
                    *to_me = me;    /* Point Parent's next ptr to "me"  */
                    me->up = extrns;    /* "my" parent  */
                    break;
                }
            }
        }
    }
}

/* ************************************************** *
 * find_extrn() - find an external reference          *
 * Passed: xtrn - starting extrn ref                  *
 *         adrs - Address to match                    *
 * ************************************************** */

struct rof_extrn *
find_extrn ( struct rof_extrn *xtrn, int adrs)
{
    int found = 0;

    while (!found)
    {
        if (adrs < xtrn->Ofst)
        {
            if (xtrn->LNext)
            {
                xtrn = xtrn->LNext;
            }
            else {
                return 0;
            }
        }
        else {
            if (adrs > xtrn->Ofst)
            {
                if (xtrn->RNext)
                {
                    xtrn = xtrn->RNext;
                }
                else {
                    return 0;
                }
            }
            else {
                if (adrs == xtrn->Ofst)
                {
                    return xtrn;
                }
            }
        }
    }

    /* Don't think we'll get here, but to prevent compiler error message */

    return 0;
}

/* ************************************************** *
 * rof_lblref() - Process a label reference found in  *
 *       the code.  On entry, Pc points to the begin  *
 *       of the reference                             *
 * Passed: pointer to int variable to store value of  *
 *      operand value                                 *
 * Returns: pointer to the rof_extern entry, with     *
 *       a label name added, if applicable            *
 * ************************************************** */

struct rof_extrn *
rof_lblref (int *value)
{
    struct rof_extrn *thisref;

    if (!(thisref = find_extrn (xtrn_code, Pc)))
    {
        return 0;
    }

    /**value = getc (progpath);
    ++Pc;*/

    if (!(thisref->Type & LOC1BYT))
    {
        *value = getc (progpath);
        ++Pc;
        /**value = (*value << 8) | getc (progpath);
        ++Pc;*/
    }
    else
    {
        *value = o9_fgetword (progpath);
        Pc += 2;
    }

    if ((!Pass2) && (!thisref->Extrn))
    {
        if (!strlen(thisref->name))
        {
            sprintf (thisref->name, "%c%04x",
                    rof_class (thisref->Type) , *value);
        }
    }

    return thisref;
}

/* ******************************************************** *
 * DataDoBlock - Process a block composed of an initialized *
 *               reference from a data area                 *
 * Passed: struct rof_extrn *mylist - pointer to tree       *
 *                      tree element                        *
 *         int datasize - the size of the area to process   *
 *         char class - the label class (D or C)            *
 * ******************************************************** */

static void
DataDoBlock (struct rof_extrn *mylist, int datasize, char class)
{
    struct rof_extrn *srch;
    struct nlist *nl;

    while (datasize > 0)
    {
        int bump = 2,
            my_val;

        if ( (nl = FindLbl (ListRoot (class), CmdEnt)) )
        {
            strcpy (pbuf->lbnm, nl->sname);

            if (nl->global)
            {
                strcat (pbuf->lbnm, ":");
            }
        }

        if ( (srch = find_extrn (mylist, CmdEnt)) )
        {

            if (srch->Type & LOC1BYT)
            {
                my_val = fgetc (progpath);
                bump = 1;
                strcpy (pbuf->mnem, "fcb");
            }
            else
            {
                my_val = o9_fgetword (progpath);
                bump = 2;
                strcpy (pbuf->mnem, "fdb");
            }

            if (strlen (srch->name))
            {
                strcpy (pbuf->operand, srch->name);
            }

            else
            {
                if ( (nl = FindLbl (ListRoot (rof_class (srch->Type)),
                                              my_val)) )
                {
                    strcpy (pbuf->operand, nl->sname);
                }
                else
                {
                    sprintf (pbuf->operand, "%c$%04x", rof_class (srch->Type),
                           my_val);
                }
            }
        }
        else {
            my_val = fgetc (progpath);
            bump = 1;
            strcpy (pbuf->mnem, "fcb");
            sprintf (pbuf->operand, "$%02x", my_val);
        }

        PrintLine (realcmd, pbuf, class, CmdEnt, (CmdEnt + datasize));
        CmdEnt += bump;
        PrevEnt = CmdEnt;
        datasize -= bump;
    }
}

static void
ROFDataLst (struct rof_extrn *mylist, int maxcount, char class)
{
    struct rof_extrn *my_ref,
                     *srch;
    int datasize;

    my_ref = mylist;

    /* First, process all entries below this one */

    if (my_ref->LNext)
    {
        ROFDataLst (my_ref->LNext, my_ref->Ofst, class);
    }

    /* ************************************** *
     * Now we've returned, process this entry *
     * ************************************** */

    /* Determine how many bytes in this block */

    if (my_ref->RNext)
    {

        srch = my_ref->RNext;

        while (srch->LNext)
        {
            srch = srch->LNext;
        }

        datasize = srch->Ofst - my_ref->Ofst;
    }
    else
    {
        datasize = maxcount - my_ref->Ofst;
    }

    CmdEnt = my_ref->Ofst;
    DataDoBlock (my_ref, datasize, class);


    if (my_ref->RNext)
    {
        ROFDataLst (my_ref->RNext, maxcount, class);
    }
}

/* ************************************************** *
 * ListInitROF() - moves initialized data into the    *
 *                 listing.  Really a setup routine   *
 * Passed: nl - Ptr to proper nlist, positioned at    *
 *               the first element to be listed       *
 *         mycount - count of elements in this sect.  *
 *         notdp   - 0 if DP, 1 if non-DP             *
 *         class   - Label Class letter               *
 * ************************************************** */

void
ListInitROF (struct nlist *nl, int mycount, int notdp, char class)
{
    struct rof_extrn *mylist,
                     *srchlst;

    /* Set up pointers, and file position */

    CmdEnt = 0;

    if (notdp)
    {
        mylist = xtrn_ndp;
        fseek (progpath, indp_begin, SEEK_SET);
    }
    else
    {
        mylist = xtrn_dp;
        fseek (progpath, idp_begin, SEEK_SET);
    }

    nl = ListRoot (class);  /* Entry point for this class's label list */

    srchlst = mylist;

    while (srchlst->LNext)
    {
        srchlst = srchlst->LNext;
    }

    if (srchlst->Ofst != 0)   /* I.E., if not 0 */
    {
        DataDoBlock (mylist, srchlst->Ofst, class);
    }

    ROFDataLst (mylist, mycount, class);
}

/* ************************************************** *
 * rofdis() - do a disassembly pass through the code  *
 * ************************************************** */

void rofdis()
{
    LinNum = PgLin = 0;

    while (Pc < (CodEnd))
    {
        register struct databndaries *bp;
        int old_pos;

        /* Try this to see if it avoids buffer overflow */
        memset (pbuf, 0, sizeof (struct printbuf));
        CmdEnt = Pc;

        /* check if in data boundary */
        if ((bp = ClasHere (dbounds, Pc)))
        {
            NsrtBnds (bp);
        }
        else {
            old_pos = ftell (progpath); /* Remember position on entry */
        }
    }
}