#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef _EXTERN_C_
#ifdef __cplusplus
#define _EXTERN_C_ extern "C"
#else /* __cplusplus */
#define _EXTERN_C_
#endif /* __cplusplus */
#endif /* _EXTERN_C_ */

#ifdef MPICH_HAS_C2F
_EXTERN_C_ void *MPIR_ToPointer(int);
#endif // MPICH_HAS_C2F

#ifdef PIC
/* For shared libraries, declare these weak and figure out which one was linked
   based on which init wrapper was called.  See mpi_init wrappers.  */
#pragma weak pmpi_init
#pragma weak PMPI_INIT
#pragma weak pmpi_init_
#pragma weak pmpi_init__
#endif /* PIC */

_EXTERN_C_ void pmpi_init(MPI_Fint *ierr);
_EXTERN_C_ void PMPI_INIT(MPI_Fint *ierr);
_EXTERN_C_ void pmpi_init_(MPI_Fint *ierr);
_EXTERN_C_ void pmpi_init__(MPI_Fint *ierr);

// -*- c++ -*-
// To build:
//    (need to use C++ because of pass-by-reference in swap)
//    wrap.py mpiarbrpn.w > mpiarbrpn.C
//    mpicc -c mpiarbrpn.C
//    ar cr libmpiarbrpn.a mpiarbrpn.o
//    ranlib libmpiarbrpn.a
#include <unistd.h>
#include <mpi.h>

#if defined(__bgq__)
#  include </bgsys/drivers/ppcfloor/spi/include/kernel/process.h>
#  include </bgsys/drivers/ppcfloor/spi/include/kernel/location.h>
#  include </bgsys/drivers/ppcfloor/firmware/include/personality.h>
#elif defined(__bgp__)
#  error Blue Gene/P support has not been implemented yet.
#elif defined(__crayxe)
#  error Cray Gemini support has not been implemented yet.
#else
#  error This only works on special supercomputers.
#endif

static MPI_Comm altworld;
static int sleep_and_abort;
static int marpn_debug;
static int in_altworld;

inline void swap_world(MPI_Comm& world) {
   if (world == MPI_COMM_NULL) {
     fprintf(stderr, "swap_world: world == MPI_COMM_NULL\n");
     fflush(stderr);
     PMPI_Abort(MPI_COMM_WORLD, 1);
   }
   if (altworld == MPI_COMM_NULL) {
     fprintf(stderr, "swap_world: altworld == MPI_COMM_NULL\n");
     fflush(stderr);
     PMPI_Abort(MPI_COMM_WORLD, 2);
   }
   if (world == MPI_COMM_WORLD) {
      world = altworld;
   }
}

/* ================== C Wrappers for MPI_Init ================== */
_EXTERN_C_ int PMPI_Init(int *arg_0, char ***arg_1);
_EXTERN_C_ int MPI_Init(int *arg_0, char ***arg_1) { 
    int _wrap_py_return_val = 0;
{
   _wrap_py_return_val = PMPI_Init(arg_0, arg_1);

   int rank;
   PMPI_Comm_rank(MPI_COMM_WORLD, &rank);

#if defined(__bgq__)
   int maxrpn = Kernel_ProcessCount();
   int rpn    = maxrpn;
#else
   int maxrpn = 1;
   int rpn    = maxrpn;
#endif

   marpn_debug = 0;
   char * dbgenv = NULL;
   dbgenv = getenv ("MARPN_DEBUG");
   if (dbgenv!=NULL)
      marpn_debug = atoi(dbgenv);

#if 1
   sleep_and_abort = 1;
#else
   sleep_and_abort = 0;
   char * sabenv = NULL;
   sabenv = getenv ("MARPN_SLEEP_AND_ABORT");
   if (sabenv!=NULL)
      sleep_and_abort = atoi(sabenv);
#endif

   char * rpnenv = NULL;
   rpnenv = getenv ("MARPN_RANKS_PER_NODE");
   if (rpnenv==NULL)
   {
      fprintf(stderr, "This is pretty boring, no?\n");
      fflush(stderr);
      PMPI_Comm_dup(MPI_COMM_WORLD, &altworld);
   }
   else 
   {
      rpn = atoi(rpnenv);
      if (rpn>maxrpn)
      {
         fprintf(stderr, "You have requested more ranks per node (%d) than are available (%d)! \n", rpn, maxrpn);
         fflush(stderr);
         PMPI_Abort(MPI_COMM_WORLD, rpn);
      }
      else if (rpn<maxrpn)
      {
         int keep = 0;

#if defined(__bgq__)
         int coreid    = Kernel_ProcessorCoreID();   /* 0-15 */
         int corehwtid = Kernel_ProcessorThreadID(); /* 0-3  */

         /* it is a good idea to spread the active ranks across all the cores 
          * one should not assume a particular rank layout when splitting world */
         if (rpn<=16)
            keep =                    ((coreid < (rpn   )) && (corehwtid == 0));
         else if (rpn<=32)
            keep = (corehwtid < 1) || ((coreid < (rpn-16)) && (corehwtid == 1));
         else if (rpn<=48)
            keep = (corehwtid < 2) || ((coreid < (rpn-32)) && (corehwtid == 2));
         else if (rpn<=64)
            keep = (corehwtid < 3) || ((coreid < (rpn-48)) && (corehwtid == 3));
         else
            keep = 0;

         if (marpn_debug)
         {
            fprintf(stderr, "rank %d (core %d, hwtid %d) is %s the new world \n", rank, coreid, corehwtid, keep ? "included in" : "excluded from" );
            fflush(stderr);
         }
#endif

         PMPI_Comm_split(MPI_COMM_WORLD, keep, rank, &altworld);
         if (!keep) 
         {
            if (sleep_and_abort)
            {
               if (marpn_debug)
               {
                  fprintf(stderr, "rank %d (core %d, hwtid %d) going to sleep \n", rank, coreid, corehwtid );
                  fflush(stderr);
               }

               /* 2 days */
               sleep(48*60*60);
            }
            else
            {
               if (marpn_debug)
               {
                  fprintf(stderr, "rank %d (core %d, hwtid %d) calling MPI_Finalize \n", rank, coreid, corehwtid );
                  fflush(stderr);
               }

               MPI_Finalize();
            }
         }
         in_altworld = keep;
      }
   } 

}    return _wrap_py_return_val;
}



/* ================== C Wrappers for MPI_Init_thread ================== */
_EXTERN_C_ int PMPI_Init_thread(int *arg_0, char ***arg_1, int arg_2, int *arg_3);
_EXTERN_C_ int MPI_Init_thread(int *arg_0, char ***arg_1, int arg_2, int *arg_3) { 
    int _wrap_py_return_val = 0;
{
   fprintf(stderr, "MARPN does not support MPI_Init_thread \n");
   fflush(stderr);
   PMPI_Abort(MPI_COMM_WORLD, 0);
}    return _wrap_py_return_val;
}



/* ================== C Wrappers for MPI_Finalize ================== */
_EXTERN_C_ int PMPI_Finalize();
_EXTERN_C_ int MPI_Finalize() { 
    int _wrap_py_return_val = 0;
{
   if (sleep_and_abort)
   {
      /* make sure all included processes reach this point */
      PMPI_Barrier(altworld);

      /* returning exit code 0 ~should~ behave like MPI_Finalize here */
      PMPI_Abort(MPI_COMM_WORLD, 0);
   }
   _wrap_py_return_val = PMPI_Finalize();
}    return _wrap_py_return_val;
}



/* ================== C Wrappers for MPI_Type_delete_attr ================== */
_EXTERN_C_ int PMPI_Type_delete_attr(MPI_Datatype arg_0, int arg_1);
_EXTERN_C_ int MPI_Type_delete_attr(MPI_Datatype arg_0, int arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_delete_attr(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_post ================== */
_EXTERN_C_ int PMPI_Win_post(MPI_Group arg_0, int arg_1, MPI_Win arg_2);
_EXTERN_C_ int MPI_Win_post(MPI_Group arg_0, int arg_1, MPI_Win arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_post(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_get_group ================== */
_EXTERN_C_ int PMPI_File_get_group(MPI_File arg_0, MPI_Group *arg_1);
_EXTERN_C_ int MPI_File_get_group(MPI_File arg_0, MPI_Group *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_get_group(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Sendrecv_replace ================== */
_EXTERN_C_ int PMPI_Sendrecv_replace(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, int arg_5, int arg_6, MPI_Comm arg_7, MPI_Status *arg_8);
_EXTERN_C_ int MPI_Sendrecv_replace(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, int arg_5, int arg_6, MPI_Comm arg_7, MPI_Status *arg_8) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_7);

   _wrap_py_return_val = PMPI_Sendrecv_replace(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Startall ================== */
_EXTERN_C_ int PMPI_Startall(int arg_0, MPI_Request *arg_1);
_EXTERN_C_ int MPI_Startall(int arg_0, MPI_Request *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Startall(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Attr_delete ================== */
_EXTERN_C_ int PMPI_Attr_delete(MPI_Comm arg_0, int arg_1);
_EXTERN_C_ int MPI_Attr_delete(MPI_Comm arg_0, int arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Attr_delete(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_get_errhandler ================== */
_EXTERN_C_ int PMPI_Win_get_errhandler(MPI_Win arg_0, MPI_Errhandler *arg_1);
_EXTERN_C_ int MPI_Win_get_errhandler(MPI_Win arg_0, MPI_Errhandler *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_get_errhandler(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_get_attr ================== */
_EXTERN_C_ int PMPI_Comm_get_attr(MPI_Comm arg_0, int arg_1, void *arg_2, int *arg_3);
_EXTERN_C_ int MPI_Comm_get_attr(MPI_Comm arg_0, int arg_1, void *arg_2, int *arg_3) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_get_attr(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_get_info ================== */
_EXTERN_C_ int PMPI_File_get_info(MPI_File arg_0, MPI_Info *arg_1);
_EXTERN_C_ int MPI_File_get_info(MPI_File arg_0, MPI_Info *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_get_info(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_write_all_begin ================== */
_EXTERN_C_ int PMPI_File_write_all_begin(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3);
_EXTERN_C_ int MPI_File_write_all_begin(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_write_all_begin(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Error_class ================== */
_EXTERN_C_ int PMPI_Error_class(int arg_0, int *arg_1);
_EXTERN_C_ int MPI_Error_class(int arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Error_class(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Free_mem ================== */
_EXTERN_C_ int PMPI_Free_mem(void *arg_0);
_EXTERN_C_ int MPI_Free_mem(void *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Free_mem(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Wtime ================== */
_EXTERN_C_ double PMPI_Wtime();
_EXTERN_C_ double MPI_Wtime() { 
    double _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Wtime();
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Info_dup ================== */
_EXTERN_C_ int PMPI_Info_dup(MPI_Info arg_0, MPI_Info *arg_1);
_EXTERN_C_ int MPI_Info_dup(MPI_Info arg_0, MPI_Info *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Info_dup(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Cart_get ================== */
_EXTERN_C_ int PMPI_Cart_get(MPI_Comm arg_0, int arg_1, int *arg_2, int *arg_3, int *arg_4);
_EXTERN_C_ int MPI_Cart_get(MPI_Comm arg_0, int arg_1, int *arg_2, int *arg_3, int *arg_4) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Cart_get(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Add_error_class ================== */
_EXTERN_C_ int PMPI_Add_error_class(int *arg_0);
_EXTERN_C_ int MPI_Add_error_class(int *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Add_error_class(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Cart_coords ================== */
_EXTERN_C_ int PMPI_Cart_coords(MPI_Comm arg_0, int arg_1, int arg_2, int *arg_3);
_EXTERN_C_ int MPI_Cart_coords(MPI_Comm arg_0, int arg_1, int arg_2, int *arg_3) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Cart_coords(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_write_shared ================== */
_EXTERN_C_ int PMPI_File_write_shared(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4);
_EXTERN_C_ int MPI_File_write_shared(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_write_shared(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Buffer_detach ================== */
_EXTERN_C_ int PMPI_Buffer_detach(void *arg_0, int *arg_1);
_EXTERN_C_ int MPI_Buffer_detach(void *arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Buffer_detach(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_set_size ================== */
_EXTERN_C_ int PMPI_File_set_size(MPI_File arg_0, MPI_Offset arg_1);
_EXTERN_C_ int MPI_File_set_size(MPI_File arg_0, MPI_Offset arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_set_size(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Intercomm_create ================== */
_EXTERN_C_ int PMPI_Intercomm_create(MPI_Comm arg_0, int arg_1, MPI_Comm arg_2, int arg_3, int arg_4, MPI_Comm *arg_5);
_EXTERN_C_ int MPI_Intercomm_create(MPI_Comm arg_0, int arg_1, MPI_Comm arg_2, int arg_3, int arg_4, MPI_Comm *arg_5) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);
swap_world(arg_2);

   _wrap_py_return_val = PMPI_Intercomm_create(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_iread_at ================== */
_EXTERN_C_ int PMPI_File_iread_at(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4, MPI_Request *arg_5);
_EXTERN_C_ int MPI_File_iread_at(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4, MPI_Request *arg_5) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_iread_at(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Allreduce ================== */
_EXTERN_C_ int PMPI_Allreduce(void *arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Op arg_4, MPI_Comm arg_5);
_EXTERN_C_ int MPI_Allreduce(void *arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Op arg_4, MPI_Comm arg_5) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Allreduce(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_create_keyval ================== */
_EXTERN_C_ int PMPI_Comm_create_keyval(MPI_Comm_copy_attr_function *arg_0, MPI_Comm_delete_attr_function *arg_1, int *arg_2, void *arg_3);
_EXTERN_C_ int MPI_Comm_create_keyval(MPI_Comm_copy_attr_function *arg_0, MPI_Comm_delete_attr_function *arg_1, int *arg_2, void *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Comm_create_keyval(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Group_rank ================== */
_EXTERN_C_ int PMPI_Group_rank(MPI_Group arg_0, int *arg_1);
_EXTERN_C_ int MPI_Group_rank(MPI_Group arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Group_rank(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_remote_size ================== */
_EXTERN_C_ int PMPI_Comm_remote_size(MPI_Comm arg_0, int *arg_1);
_EXTERN_C_ int MPI_Comm_remote_size(MPI_Comm arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_remote_size(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_contiguous ================== */
_EXTERN_C_ int PMPI_Type_contiguous(int arg_0, MPI_Datatype arg_1, MPI_Datatype *arg_2);
_EXTERN_C_ int MPI_Type_contiguous(int arg_0, MPI_Datatype arg_1, MPI_Datatype *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_contiguous(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Send_init ================== */
_EXTERN_C_ int PMPI_Send_init(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6);
_EXTERN_C_ int MPI_Send_init(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Send_init(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_delete ================== */
_EXTERN_C_ int PMPI_File_delete(char *arg_0, MPI_Info arg_1);
_EXTERN_C_ int MPI_File_delete(char *arg_0, MPI_Info arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_delete(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_get_name ================== */
_EXTERN_C_ int PMPI_Type_get_name(MPI_Datatype arg_0, char *arg_1, int *arg_2);
_EXTERN_C_ int MPI_Type_get_name(MPI_Datatype arg_0, char *arg_1, int *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_get_name(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Reduce ================== */
_EXTERN_C_ int PMPI_Reduce(void *arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Op arg_4, int arg_5, MPI_Comm arg_6);
_EXTERN_C_ int MPI_Reduce(void *arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Op arg_4, int arg_5, MPI_Comm arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_6);

   _wrap_py_return_val = PMPI_Reduce(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_commit ================== */
_EXTERN_C_ int PMPI_Type_commit(MPI_Datatype *arg_0);
_EXTERN_C_ int MPI_Type_commit(MPI_Datatype *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_commit(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_get_parent ================== */
_EXTERN_C_ int PMPI_Comm_get_parent(MPI_Comm *arg_0);
_EXTERN_C_ int MPI_Comm_get_parent(MPI_Comm *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Comm_get_parent(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_create_f90_integer ================== */
_EXTERN_C_ int PMPI_Type_create_f90_integer(int arg_0, MPI_Datatype *arg_1);
_EXTERN_C_ int MPI_Type_create_f90_integer(int arg_0, MPI_Datatype *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_create_f90_integer(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_hindexed ================== */
_EXTERN_C_ int PMPI_Type_hindexed(int arg_0, int *arg_1, MPI_Aint *arg_2, MPI_Datatype arg_3, MPI_Datatype *arg_4);
_EXTERN_C_ int MPI_Type_hindexed(int arg_0, int *arg_1, MPI_Aint *arg_2, MPI_Datatype arg_3, MPI_Datatype *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_hindexed(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_extent ================== */
_EXTERN_C_ int PMPI_Type_extent(MPI_Datatype arg_0, MPI_Aint *arg_1);
_EXTERN_C_ int MPI_Type_extent(MPI_Datatype arg_0, MPI_Aint *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_extent(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Lookup_name ================== */
_EXTERN_C_ int PMPI_Lookup_name(char *arg_0, MPI_Info arg_1, char *arg_2);
_EXTERN_C_ int MPI_Lookup_name(char *arg_0, MPI_Info arg_1, char *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Lookup_name(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_get_position ================== */
_EXTERN_C_ int PMPI_File_get_position(MPI_File arg_0, MPI_Offset *arg_1);
_EXTERN_C_ int MPI_File_get_position(MPI_File arg_0, MPI_Offset *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_get_position(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Scan ================== */
_EXTERN_C_ int PMPI_Scan(void *arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Op arg_4, MPI_Comm arg_5);
_EXTERN_C_ int MPI_Scan(void *arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Op arg_4, MPI_Comm arg_5) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Scan(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Recv ================== */
_EXTERN_C_ int PMPI_Recv(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Status *arg_6);
_EXTERN_C_ int MPI_Recv(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Status *arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Recv(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Pcontrol ================== */
_EXTERN_C_ int PMPI_Pcontrol(const int arg_0, ...);
_EXTERN_C_ int MPI_Pcontrol(const int arg_0, ...) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Pcontrol(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Group_translate_ranks ================== */
_EXTERN_C_ int PMPI_Group_translate_ranks(MPI_Group arg_0, int arg_1, int *arg_2, MPI_Group arg_3, int *arg_4);
_EXTERN_C_ int MPI_Group_translate_ranks(MPI_Group arg_0, int arg_1, int *arg_2, MPI_Group arg_3, int *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Group_translate_ranks(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Testsome ================== */
_EXTERN_C_ int PMPI_Testsome(int arg_0, MPI_Request *arg_1, int *arg_2, int *arg_3, MPI_Status *arg_4);
_EXTERN_C_ int MPI_Testsome(int arg_0, MPI_Request *arg_1, int *arg_2, int *arg_3, MPI_Status *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Testsome(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Recv_init ================== */
_EXTERN_C_ int PMPI_Recv_init(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6);
_EXTERN_C_ int MPI_Recv_init(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Recv_init(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_set_name ================== */
_EXTERN_C_ int PMPI_Win_set_name(MPI_Win arg_0, char *arg_1);
_EXTERN_C_ int MPI_Win_set_name(MPI_Win arg_0, char *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_set_name(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_dup ================== */
_EXTERN_C_ int PMPI_Type_dup(MPI_Datatype arg_0, MPI_Datatype *arg_1);
_EXTERN_C_ int MPI_Type_dup(MPI_Datatype arg_0, MPI_Datatype *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_dup(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_write ================== */
_EXTERN_C_ int PMPI_File_write(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4);
_EXTERN_C_ int MPI_File_write(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_write(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_group ================== */
_EXTERN_C_ int PMPI_Comm_group(MPI_Comm arg_0, MPI_Group *arg_1);
_EXTERN_C_ int MPI_Comm_group(MPI_Comm arg_0, MPI_Group *arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_group(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Add_error_code ================== */
_EXTERN_C_ int PMPI_Add_error_code(int arg_0, int *arg_1);
_EXTERN_C_ int MPI_Add_error_code(int arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Add_error_code(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_create_resized ================== */
_EXTERN_C_ int PMPI_Type_create_resized(MPI_Datatype arg_0, MPI_Aint arg_1, MPI_Aint arg_2, MPI_Datatype *arg_3);
_EXTERN_C_ int MPI_Type_create_resized(MPI_Datatype arg_0, MPI_Aint arg_1, MPI_Aint arg_2, MPI_Datatype *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_create_resized(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_iwrite_shared ================== */
_EXTERN_C_ int PMPI_File_iwrite_shared(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Request *arg_4);
_EXTERN_C_ int MPI_File_iwrite_shared(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Request *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_iwrite_shared(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Get_address ================== */
_EXTERN_C_ int PMPI_Get_address(void *arg_0, MPI_Aint *arg_1);
_EXTERN_C_ int MPI_Get_address(void *arg_0, MPI_Aint *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Get_address(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Is_thread_main ================== */
_EXTERN_C_ int PMPI_Is_thread_main(int *arg_0);
_EXTERN_C_ int MPI_Is_thread_main(int *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Is_thread_main(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Put ================== */
_EXTERN_C_ int PMPI_Put(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, MPI_Aint arg_4, int arg_5, MPI_Datatype arg_6, MPI_Win arg_7);
_EXTERN_C_ int MPI_Put(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, MPI_Aint arg_4, int arg_5, MPI_Datatype arg_6, MPI_Win arg_7) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Put(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_create_indexed_block ================== */
_EXTERN_C_ int PMPI_Type_create_indexed_block(int arg_0, int arg_1, int arg_2[], MPI_Datatype arg_3, MPI_Datatype *arg_4);
_EXTERN_C_ int MPI_Type_create_indexed_block(int arg_0, int arg_1, int arg_2[], MPI_Datatype arg_3, MPI_Datatype *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_create_indexed_block(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Initialized ================== */
_EXTERN_C_ int PMPI_Initialized(int *arg_0);
_EXTERN_C_ int MPI_Initialized(int *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Initialized(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_iwrite ================== */
_EXTERN_C_ int PMPI_File_iwrite(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Request *arg_4);
_EXTERN_C_ int MPI_File_iwrite(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Request *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_iwrite(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Dist_graph_neighbors ================== */
_EXTERN_C_ int PMPI_Dist_graph_neighbors(MPI_Comm comm, int maxindegree, int arg_2[], int arg_3[], int maxoutdegree, int arg_5[], int arg_6[]);
_EXTERN_C_ int MPI_Dist_graph_neighbors(MPI_Comm comm, int maxindegree, int arg_2[], int arg_3[], int maxoutdegree, int arg_5[], int arg_6[]) { 
    int _wrap_py_return_val = 0;
{
   swap_world(comm);

   _wrap_py_return_val = PMPI_Dist_graph_neighbors(comm, maxindegree, arg_2, arg_3, maxoutdegree, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Bsend ================== */
_EXTERN_C_ int PMPI_Bsend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5);
_EXTERN_C_ int MPI_Bsend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Bsend(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Errhandler_get ================== */
_EXTERN_C_ int PMPI_Errhandler_get(MPI_Comm arg_0, MPI_Errhandler *arg_1);
_EXTERN_C_ int MPI_Errhandler_get(MPI_Comm arg_0, MPI_Errhandler *arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Errhandler_get(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Get_count ================== */
_EXTERN_C_ int PMPI_Get_count(MPI_Status *arg_0, MPI_Datatype arg_1, int *arg_2);
_EXTERN_C_ int MPI_Get_count(MPI_Status *arg_0, MPI_Datatype arg_1, int *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Get_count(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Error_string ================== */
_EXTERN_C_ int PMPI_Error_string(int arg_0, char *arg_1, int *arg_2);
_EXTERN_C_ int MPI_Error_string(int arg_0, char *arg_1, int *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Error_string(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Grequest_start ================== */
_EXTERN_C_ int PMPI_Grequest_start(MPI_Grequest_query_function *arg_0, MPI_Grequest_free_function *arg_1, MPI_Grequest_cancel_function *arg_2, void *arg_3, MPI_Request *arg_4);
_EXTERN_C_ int MPI_Grequest_start(MPI_Grequest_query_function *arg_0, MPI_Grequest_free_function *arg_1, MPI_Grequest_cancel_function *arg_2, void *arg_3, MPI_Request *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Grequest_start(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Cartdim_get ================== */
_EXTERN_C_ int PMPI_Cartdim_get(MPI_Comm arg_0, int *arg_1);
_EXTERN_C_ int MPI_Cartdim_get(MPI_Comm arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Cartdim_get(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Allgather ================== */
_EXTERN_C_ int PMPI_Allgather(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int arg_4, MPI_Datatype arg_5, MPI_Comm arg_6);
_EXTERN_C_ int MPI_Allgather(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int arg_4, MPI_Datatype arg_5, MPI_Comm arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_6);

   _wrap_py_return_val = PMPI_Allgather(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Irecv ================== */
_EXTERN_C_ int PMPI_Irecv(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6);
_EXTERN_C_ int MPI_Irecv(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Irecv(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Issend ================== */
_EXTERN_C_ int PMPI_Issend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6);
_EXTERN_C_ int MPI_Issend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Issend(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_sync ================== */
_EXTERN_C_ int PMPI_File_sync(MPI_File arg_0);
_EXTERN_C_ int MPI_File_sync(MPI_File arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_sync(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Rsend ================== */
_EXTERN_C_ int PMPI_Rsend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5);
_EXTERN_C_ int MPI_Rsend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Rsend(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_get_amode ================== */
_EXTERN_C_ int PMPI_File_get_amode(MPI_File arg_0, int *arg_1);
_EXTERN_C_ int MPI_File_get_amode(MPI_File arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_get_amode(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Abort ================== */
_EXTERN_C_ int PMPI_Abort(MPI_Comm arg_0, int arg_1);
_EXTERN_C_ int MPI_Abort(MPI_Comm arg_0, int arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Abort(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Grequest_complete ================== */
_EXTERN_C_ int PMPI_Grequest_complete(MPI_Request arg_0);
_EXTERN_C_ int MPI_Grequest_complete(MPI_Request arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Grequest_complete(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Pack ================== */
_EXTERN_C_ int PMPI_Pack(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int arg_4, int *arg_5, MPI_Comm arg_6);
_EXTERN_C_ int MPI_Pack(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int arg_4, int *arg_5, MPI_Comm arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_6);

   _wrap_py_return_val = PMPI_Pack(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_set_attr ================== */
_EXTERN_C_ int PMPI_Win_set_attr(MPI_Win arg_0, int arg_1, void *arg_2);
_EXTERN_C_ int MPI_Win_set_attr(MPI_Win arg_0, int arg_1, void *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_set_attr(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Info_create ================== */
_EXTERN_C_ int PMPI_Info_create(MPI_Info *arg_0);
_EXTERN_C_ int MPI_Info_create(MPI_Info *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Info_create(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_create_errhandler ================== */
_EXTERN_C_ int PMPI_Win_create_errhandler(MPI_Win_errhandler_function *arg_0, MPI_Errhandler *arg_1);
_EXTERN_C_ int MPI_Win_create_errhandler(MPI_Win_errhandler_function *arg_0, MPI_Errhandler *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_create_errhandler(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_create_f90_complex ================== */
_EXTERN_C_ int PMPI_Type_create_f90_complex(int arg_0, int arg_1, MPI_Datatype *arg_2);
_EXTERN_C_ int MPI_Type_create_f90_complex(int arg_0, int arg_1, MPI_Datatype *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_create_f90_complex(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_open ================== */
_EXTERN_C_ int PMPI_File_open(MPI_Comm arg_0, char *arg_1, int arg_2, MPI_Info arg_3, MPI_File *arg_4);
_EXTERN_C_ int MPI_File_open(MPI_Comm arg_0, char *arg_1, int arg_2, MPI_Info arg_3, MPI_File *arg_4) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_File_open(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Info_get_nkeys ================== */
_EXTERN_C_ int PMPI_Info_get_nkeys(MPI_Info arg_0, int *arg_1);
_EXTERN_C_ int MPI_Info_get_nkeys(MPI_Info arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Info_get_nkeys(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Gatherv ================== */
_EXTERN_C_ int PMPI_Gatherv(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int *arg_4, int *arg_5, MPI_Datatype arg_6, int arg_7, MPI_Comm arg_8);
_EXTERN_C_ int MPI_Gatherv(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int *arg_4, int *arg_5, MPI_Datatype arg_6, int arg_7, MPI_Comm arg_8) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_8);

   _wrap_py_return_val = PMPI_Gatherv(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_get_attr ================== */
_EXTERN_C_ int PMPI_Type_get_attr(MPI_Datatype arg_0, int arg_1, void *arg_2, int *arg_3);
_EXTERN_C_ int MPI_Type_get_attr(MPI_Datatype arg_0, int arg_1, void *arg_2, int *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_get_attr(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_create ================== */
_EXTERN_C_ int PMPI_Comm_create(MPI_Comm arg_0, MPI_Group arg_1, MPI_Comm *arg_2);
_EXTERN_C_ int MPI_Comm_create(MPI_Comm arg_0, MPI_Group arg_1, MPI_Comm *arg_2) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_create(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Send ================== */
_EXTERN_C_ int PMPI_Send(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5);
_EXTERN_C_ int MPI_Send(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Send(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_disconnect ================== */
_EXTERN_C_ int PMPI_Comm_disconnect(MPI_Comm *arg_0);
_EXTERN_C_ int MPI_Comm_disconnect(MPI_Comm *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Comm_disconnect(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_remote_group ================== */
_EXTERN_C_ int PMPI_Comm_remote_group(MPI_Comm arg_0, MPI_Group *arg_1);
_EXTERN_C_ int MPI_Comm_remote_group(MPI_Comm arg_0, MPI_Group *arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_remote_group(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Cart_shift ================== */
_EXTERN_C_ int PMPI_Cart_shift(MPI_Comm arg_0, int arg_1, int arg_2, int *arg_3, int *arg_4);
_EXTERN_C_ int MPI_Cart_shift(MPI_Comm arg_0, int arg_1, int arg_2, int *arg_3, int *arg_4) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Cart_shift(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_size ================== */
_EXTERN_C_ int PMPI_Comm_size(MPI_Comm arg_0, int *arg_1);
_EXTERN_C_ int MPI_Comm_size(MPI_Comm arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_size(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_get_envelope ================== */
_EXTERN_C_ int PMPI_Type_get_envelope(MPI_Datatype arg_0, int *arg_1, int *arg_2, int *arg_3, int *arg_4);
_EXTERN_C_ int MPI_Type_get_envelope(MPI_Datatype arg_0, int *arg_1, int *arg_2, int *arg_3, int *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_get_envelope(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_iread_shared ================== */
_EXTERN_C_ int PMPI_File_iread_shared(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Request *arg_4);
_EXTERN_C_ int MPI_File_iread_shared(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Request *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_iread_shared(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_set_errhandler ================== */
_EXTERN_C_ int PMPI_File_set_errhandler(MPI_File arg_0, MPI_Errhandler arg_1);
_EXTERN_C_ int MPI_File_set_errhandler(MPI_File arg_0, MPI_Errhandler arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_set_errhandler(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_fence ================== */
_EXTERN_C_ int PMPI_Win_fence(int arg_0, MPI_Win arg_1);
_EXTERN_C_ int MPI_Win_fence(int arg_0, MPI_Win arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_fence(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Request_free ================== */
_EXTERN_C_ int PMPI_Request_free(MPI_Request *arg_0);
_EXTERN_C_ int MPI_Request_free(MPI_Request *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Request_free(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Waitsome ================== */
_EXTERN_C_ int PMPI_Waitsome(int arg_0, MPI_Request *arg_1, int *arg_2, int *arg_3, MPI_Status *arg_4);
_EXTERN_C_ int MPI_Waitsome(int arg_0, MPI_Request *arg_1, int *arg_2, int *arg_3, MPI_Status *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Waitsome(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Group_difference ================== */
_EXTERN_C_ int PMPI_Group_difference(MPI_Group arg_0, MPI_Group arg_1, MPI_Group *arg_2);
_EXTERN_C_ int MPI_Group_difference(MPI_Group arg_0, MPI_Group arg_1, MPI_Group *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Group_difference(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_read_all_end ================== */
_EXTERN_C_ int PMPI_File_read_all_end(MPI_File arg_0, void *arg_1, MPI_Status *arg_2);
_EXTERN_C_ int MPI_File_read_all_end(MPI_File arg_0, void *arg_1, MPI_Status *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_read_all_end(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Reduce_scatter ================== */
_EXTERN_C_ int PMPI_Reduce_scatter(void *arg_0, void *arg_1, int *arg_2, MPI_Datatype arg_3, MPI_Op arg_4, MPI_Comm arg_5);
_EXTERN_C_ int MPI_Reduce_scatter(void *arg_0, void *arg_1, int *arg_2, MPI_Datatype arg_3, MPI_Op arg_4, MPI_Comm arg_5) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Reduce_scatter(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_get_name ================== */
_EXTERN_C_ int PMPI_Win_get_name(MPI_Win arg_0, char *arg_1, int *arg_2);
_EXTERN_C_ int MPI_Win_get_name(MPI_Win arg_0, char *arg_1, int *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_get_name(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_create_f90_real ================== */
_EXTERN_C_ int PMPI_Type_create_f90_real(int arg_0, int arg_1, MPI_Datatype *arg_2);
_EXTERN_C_ int MPI_Type_create_f90_real(int arg_0, int arg_1, MPI_Datatype *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_create_f90_real(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Wait ================== */
_EXTERN_C_ int PMPI_Wait(MPI_Request *arg_0, MPI_Status *arg_1);
_EXTERN_C_ int MPI_Wait(MPI_Request *arg_0, MPI_Status *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Wait(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_get_errhandler ================== */
_EXTERN_C_ int PMPI_File_get_errhandler(MPI_File arg_0, MPI_Errhandler *arg_1);
_EXTERN_C_ int MPI_File_get_errhandler(MPI_File arg_0, MPI_Errhandler *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_get_errhandler(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Group_incl ================== */
_EXTERN_C_ int PMPI_Group_incl(MPI_Group arg_0, int arg_1, int *arg_2, MPI_Group *arg_3);
_EXTERN_C_ int MPI_Group_incl(MPI_Group arg_0, int arg_1, int *arg_2, MPI_Group *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Group_incl(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Irsend ================== */
_EXTERN_C_ int PMPI_Irsend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6);
_EXTERN_C_ int MPI_Irsend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Irsend(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Scatterv ================== */
_EXTERN_C_ int PMPI_Scatterv(void *arg_0, int *arg_1, int *arg_2, MPI_Datatype arg_3, void *arg_4, int arg_5, MPI_Datatype arg_6, int arg_7, MPI_Comm arg_8);
_EXTERN_C_ int MPI_Scatterv(void *arg_0, int *arg_1, int *arg_2, MPI_Datatype arg_3, void *arg_4, int arg_5, MPI_Datatype arg_6, int arg_7, MPI_Comm arg_8) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_8);

   _wrap_py_return_val = PMPI_Scatterv(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_call_errhandler ================== */
_EXTERN_C_ int PMPI_File_call_errhandler(MPI_File arg_0, int arg_1);
_EXTERN_C_ int MPI_File_call_errhandler(MPI_File arg_0, int arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_call_errhandler(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_create_errhandler ================== */
_EXTERN_C_ int PMPI_Comm_create_errhandler(MPI_Comm_errhandler_function *arg_0, MPI_Errhandler *arg_1);
_EXTERN_C_ int MPI_Comm_create_errhandler(MPI_Comm_errhandler_function *arg_0, MPI_Errhandler *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Comm_create_errhandler(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_read_shared ================== */
_EXTERN_C_ int PMPI_File_read_shared(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4);
_EXTERN_C_ int MPI_File_read_shared(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_read_shared(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_write_all ================== */
_EXTERN_C_ int PMPI_File_write_all(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4);
_EXTERN_C_ int MPI_File_write_all(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_write_all(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_connect ================== */
_EXTERN_C_ int PMPI_Comm_connect(char *arg_0, MPI_Info arg_1, int arg_2, MPI_Comm arg_3, MPI_Comm *arg_4);
_EXTERN_C_ int MPI_Comm_connect(char *arg_0, MPI_Info arg_1, int arg_2, MPI_Comm arg_3, MPI_Comm *arg_4) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_3);

   _wrap_py_return_val = PMPI_Comm_connect(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Group_compare ================== */
_EXTERN_C_ int PMPI_Group_compare(MPI_Group arg_0, MPI_Group arg_1, int *arg_2);
_EXTERN_C_ int MPI_Group_compare(MPI_Group arg_0, MPI_Group arg_1, int *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Group_compare(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Address ================== */
_EXTERN_C_ int PMPI_Address(void *arg_0, MPI_Aint *arg_1);
_EXTERN_C_ int MPI_Address(void *arg_0, MPI_Aint *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Address(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_compare ================== */
_EXTERN_C_ int PMPI_Comm_compare(MPI_Comm arg_0, MPI_Comm arg_1, int *arg_2);
_EXTERN_C_ int MPI_Comm_compare(MPI_Comm arg_0, MPI_Comm arg_1, int *arg_2) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);
swap_world(arg_1);

   _wrap_py_return_val = PMPI_Comm_compare(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_unlock ================== */
_EXTERN_C_ int PMPI_Win_unlock(int arg_0, MPI_Win arg_1);
_EXTERN_C_ int MPI_Win_unlock(int arg_0, MPI_Win arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_unlock(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Pack_external ================== */
_EXTERN_C_ int PMPI_Pack_external(char *arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, void *arg_4, MPI_Aint arg_5, MPI_Aint *arg_6);
_EXTERN_C_ int MPI_Pack_external(char *arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, void *arg_4, MPI_Aint arg_5, MPI_Aint *arg_6) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Pack_external(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Test_cancelled ================== */
_EXTERN_C_ int PMPI_Test_cancelled(MPI_Status *arg_0, int *arg_1);
_EXTERN_C_ int MPI_Test_cancelled(MPI_Status *arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Test_cancelled(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Attr_get ================== */
_EXTERN_C_ int PMPI_Attr_get(MPI_Comm arg_0, int arg_1, void *arg_2, int *arg_3);
_EXTERN_C_ int MPI_Attr_get(MPI_Comm arg_0, int arg_1, void *arg_2, int *arg_3) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Attr_get(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_read ================== */
_EXTERN_C_ int PMPI_File_read(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4);
_EXTERN_C_ int MPI_File_read(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_read(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Query_thread ================== */
_EXTERN_C_ int PMPI_Query_thread(int *arg_0);
_EXTERN_C_ int MPI_Query_thread(int *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Query_thread(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_call_errhandler ================== */
_EXTERN_C_ int PMPI_Win_call_errhandler(MPI_Win arg_0, int arg_1);
_EXTERN_C_ int MPI_Win_call_errhandler(MPI_Win arg_0, int arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_call_errhandler(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_get_group ================== */
_EXTERN_C_ int PMPI_Win_get_group(MPI_Win arg_0, MPI_Group *arg_1);
_EXTERN_C_ int MPI_Win_get_group(MPI_Win arg_0, MPI_Group *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_get_group(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Errhandler_create ================== */
_EXTERN_C_ int PMPI_Errhandler_create(MPI_Handler_function *arg_0, MPI_Errhandler *arg_1);
_EXTERN_C_ int MPI_Errhandler_create(MPI_Handler_function *arg_0, MPI_Errhandler *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Errhandler_create(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Cart_create ================== */
_EXTERN_C_ int PMPI_Cart_create(MPI_Comm arg_0, int arg_1, int *arg_2, int *arg_3, int arg_4, MPI_Comm *arg_5);
_EXTERN_C_ int MPI_Cart_create(MPI_Comm arg_0, int arg_1, int *arg_2, int *arg_3, int arg_4, MPI_Comm *arg_5) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Cart_create(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Status_set_cancelled ================== */
_EXTERN_C_ int PMPI_Status_set_cancelled(MPI_Status *arg_0, int arg_1);
_EXTERN_C_ int MPI_Status_set_cancelled(MPI_Status *arg_0, int arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Status_set_cancelled(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_get_attr ================== */
_EXTERN_C_ int PMPI_Win_get_attr(MPI_Win arg_0, int arg_1, void *arg_2, int *arg_3);
_EXTERN_C_ int MPI_Win_get_attr(MPI_Win arg_0, int arg_1, void *arg_2, int *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_get_attr(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_struct ================== */
_EXTERN_C_ int PMPI_Type_struct(int arg_0, int *arg_1, MPI_Aint *arg_2, MPI_Datatype *arg_3, MPI_Datatype *arg_4);
_EXTERN_C_ int MPI_Type_struct(int arg_0, int *arg_1, MPI_Aint *arg_2, MPI_Datatype *arg_3, MPI_Datatype *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_struct(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Graph_neighbors_count ================== */
_EXTERN_C_ int PMPI_Graph_neighbors_count(MPI_Comm arg_0, int arg_1, int *arg_2);
_EXTERN_C_ int MPI_Graph_neighbors_count(MPI_Comm arg_0, int arg_1, int *arg_2) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Graph_neighbors_count(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Open_port ================== */
_EXTERN_C_ int PMPI_Open_port(MPI_Info arg_0, char *arg_1);
_EXTERN_C_ int MPI_Open_port(MPI_Info arg_0, char *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Open_port(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Allgatherv ================== */
_EXTERN_C_ int PMPI_Allgatherv(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int *arg_4, int *arg_5, MPI_Datatype arg_6, MPI_Comm arg_7);
_EXTERN_C_ int MPI_Allgatherv(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int *arg_4, int *arg_5, MPI_Datatype arg_6, MPI_Comm arg_7) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_7);

   _wrap_py_return_val = PMPI_Allgatherv(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Sendrecv ================== */
_EXTERN_C_ int PMPI_Sendrecv(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, void *arg_5, int arg_6, MPI_Datatype arg_7, int arg_8, int arg_9, MPI_Comm arg_10, MPI_Status *arg_11);
_EXTERN_C_ int MPI_Sendrecv(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, void *arg_5, int arg_6, MPI_Datatype arg_7, int arg_8, int arg_9, MPI_Comm arg_10, MPI_Status *arg_11) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_10);

   _wrap_py_return_val = PMPI_Sendrecv(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8, arg_9, arg_10, arg_11);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_get_position_shared ================== */
_EXTERN_C_ int PMPI_File_get_position_shared(MPI_File arg_0, MPI_Offset *arg_1);
_EXTERN_C_ int MPI_File_get_position_shared(MPI_File arg_0, MPI_Offset *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_get_position_shared(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Graph_neighbors ================== */
_EXTERN_C_ int PMPI_Graph_neighbors(MPI_Comm arg_0, int arg_1, int arg_2, int *arg_3);
_EXTERN_C_ int MPI_Graph_neighbors(MPI_Comm arg_0, int arg_1, int arg_2, int *arg_3) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Graph_neighbors(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_delete_attr ================== */
_EXTERN_C_ int PMPI_Comm_delete_attr(MPI_Comm arg_0, int arg_1);
_EXTERN_C_ int MPI_Comm_delete_attr(MPI_Comm arg_0, int arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_delete_attr(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Dims_create ================== */
_EXTERN_C_ int PMPI_Dims_create(int arg_0, int arg_1, int *arg_2);
_EXTERN_C_ int MPI_Dims_create(int arg_0, int arg_1, int *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Dims_create(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_iread ================== */
_EXTERN_C_ int PMPI_File_iread(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Request *arg_4);
_EXTERN_C_ int MPI_File_iread(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Request *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_iread(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Scatter ================== */
_EXTERN_C_ int PMPI_Scatter(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int arg_4, MPI_Datatype arg_5, int arg_6, MPI_Comm arg_7);
_EXTERN_C_ int MPI_Scatter(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int arg_4, MPI_Datatype arg_5, int arg_6, MPI_Comm arg_7) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_7);

   _wrap_py_return_val = PMPI_Scatter(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_set_attr ================== */
_EXTERN_C_ int PMPI_Comm_set_attr(MPI_Comm arg_0, int arg_1, void *arg_2);
_EXTERN_C_ int MPI_Comm_set_attr(MPI_Comm arg_0, int arg_1, void *arg_2) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_set_attr(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_write_at ================== */
_EXTERN_C_ int PMPI_File_write_at(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4, MPI_Status *arg_5);
_EXTERN_C_ int MPI_File_write_at(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4, MPI_Status *arg_5) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_write_at(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_free_keyval ================== */
_EXTERN_C_ int PMPI_Comm_free_keyval(int *arg_0);
_EXTERN_C_ int MPI_Comm_free_keyval(int *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Comm_free_keyval(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Add_error_string ================== */
_EXTERN_C_ int PMPI_Add_error_string(int arg_0, char *arg_1);
_EXTERN_C_ int MPI_Add_error_string(int arg_0, char *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Add_error_string(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Op_create ================== */
_EXTERN_C_ int PMPI_Op_create(MPI_User_function *arg_0, int arg_1, MPI_Op *arg_2);
_EXTERN_C_ int MPI_Op_create(MPI_User_function *arg_0, int arg_1, MPI_Op *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Op_create(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_set_name ================== */
_EXTERN_C_ int PMPI_Type_set_name(MPI_Datatype arg_0, char *arg_1);
_EXTERN_C_ int MPI_Type_set_name(MPI_Datatype arg_0, char *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_set_name(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Exscan ================== */
_EXTERN_C_ int PMPI_Exscan(void *arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Op arg_4, MPI_Comm arg_5);
_EXTERN_C_ int MPI_Exscan(void *arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Op arg_4, MPI_Comm arg_5) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Exscan(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Ssend_init ================== */
_EXTERN_C_ int PMPI_Ssend_init(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6);
_EXTERN_C_ int MPI_Ssend_init(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Ssend_init(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Rsend_init ================== */
_EXTERN_C_ int PMPI_Rsend_init(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6);
_EXTERN_C_ int MPI_Rsend_init(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Rsend_init(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Info_free ================== */
_EXTERN_C_ int PMPI_Info_free(MPI_Info *info);
_EXTERN_C_ int MPI_Info_free(MPI_Info *info) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Info_free(info);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Publish_name ================== */
_EXTERN_C_ int PMPI_Publish_name(char *arg_0, MPI_Info arg_1, char *arg_2);
_EXTERN_C_ int MPI_Publish_name(char *arg_0, MPI_Info arg_1, char *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Publish_name(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Bcast ================== */
_EXTERN_C_ int PMPI_Bcast(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, MPI_Comm arg_4);
_EXTERN_C_ int MPI_Bcast(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, MPI_Comm arg_4) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_4);

   _wrap_py_return_val = PMPI_Bcast(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_read_at_all ================== */
_EXTERN_C_ int PMPI_File_read_at_all(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4, MPI_Status *arg_5);
_EXTERN_C_ int MPI_File_read_at_all(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4, MPI_Status *arg_5) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_read_at_all(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_get_extent ================== */
_EXTERN_C_ int PMPI_Type_get_extent(MPI_Datatype arg_0, MPI_Aint *arg_1, MPI_Aint *arg_2);
_EXTERN_C_ int MPI_Type_get_extent(MPI_Datatype arg_0, MPI_Aint *arg_1, MPI_Aint *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_get_extent(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_write_ordered_end ================== */
_EXTERN_C_ int PMPI_File_write_ordered_end(MPI_File arg_0, void *arg_1, MPI_Status *arg_2);
_EXTERN_C_ int MPI_File_write_ordered_end(MPI_File arg_0, void *arg_1, MPI_Status *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_write_ordered_end(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Get_version ================== */
_EXTERN_C_ int PMPI_Get_version(int *arg_0, int *arg_1);
_EXTERN_C_ int MPI_Get_version(int *arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Get_version(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_create_errhandler ================== */
_EXTERN_C_ int PMPI_File_create_errhandler(MPI_File_errhandler_function *arg_0, MPI_Errhandler *arg_1);
_EXTERN_C_ int MPI_File_create_errhandler(MPI_File_errhandler_function *arg_0, MPI_Errhandler *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_create_errhandler(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Graph_create ================== */
_EXTERN_C_ int PMPI_Graph_create(MPI_Comm arg_0, int arg_1, int *arg_2, int *arg_3, int arg_4, MPI_Comm *arg_5);
_EXTERN_C_ int MPI_Graph_create(MPI_Comm arg_0, int arg_1, int *arg_2, int *arg_3, int arg_4, MPI_Comm *arg_5) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Graph_create(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_free ================== */
_EXTERN_C_ int PMPI_Comm_free(MPI_Comm *arg_0);
_EXTERN_C_ int MPI_Comm_free(MPI_Comm *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Comm_free(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_write_at_all ================== */
_EXTERN_C_ int PMPI_File_write_at_all(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4, MPI_Status *arg_5);
_EXTERN_C_ int MPI_File_write_at_all(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4, MPI_Status *arg_5) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_write_at_all(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Group_excl ================== */
_EXTERN_C_ int PMPI_Group_excl(MPI_Group arg_0, int arg_1, int *arg_2, MPI_Group *arg_3);
_EXTERN_C_ int MPI_Group_excl(MPI_Group arg_0, int arg_1, int *arg_2, MPI_Group *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Group_excl(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Pack_size ================== */
_EXTERN_C_ int PMPI_Pack_size(int arg_0, MPI_Datatype arg_1, MPI_Comm arg_2, int *arg_3);
_EXTERN_C_ int MPI_Pack_size(int arg_0, MPI_Datatype arg_1, MPI_Comm arg_2, int *arg_3) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_2);

   _wrap_py_return_val = PMPI_Pack_size(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Ibsend ================== */
_EXTERN_C_ int PMPI_Ibsend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6);
_EXTERN_C_ int MPI_Ibsend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Ibsend(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_test_inter ================== */
_EXTERN_C_ int PMPI_Comm_test_inter(MPI_Comm arg_0, int *arg_1);
_EXTERN_C_ int MPI_Comm_test_inter(MPI_Comm arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_test_inter(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Intercomm_merge ================== */
_EXTERN_C_ int PMPI_Intercomm_merge(MPI_Comm arg_0, int arg_1, MPI_Comm *arg_2);
_EXTERN_C_ int MPI_Intercomm_merge(MPI_Comm arg_0, int arg_1, MPI_Comm *arg_2) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Intercomm_merge(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_complete ================== */
_EXTERN_C_ int PMPI_Win_complete(MPI_Win arg_0);
_EXTERN_C_ int MPI_Win_complete(MPI_Win arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_complete(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_get_type_extent ================== */
_EXTERN_C_ int PMPI_File_get_type_extent(MPI_File arg_0, MPI_Datatype arg_1, MPI_Aint *arg_2);
_EXTERN_C_ int MPI_File_get_type_extent(MPI_File arg_0, MPI_Datatype arg_1, MPI_Aint *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_get_type_extent(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Graph_map ================== */
_EXTERN_C_ int PMPI_Graph_map(MPI_Comm arg_0, int arg_1, int *arg_2, int *arg_3, int *arg_4);
_EXTERN_C_ int MPI_Graph_map(MPI_Comm arg_0, int arg_1, int *arg_2, int *arg_3, int *arg_4) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Graph_map(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_read_all_begin ================== */
_EXTERN_C_ int PMPI_File_read_all_begin(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3);
_EXTERN_C_ int MPI_File_read_all_begin(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_read_all_begin(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Info_get_valuelen ================== */
_EXTERN_C_ int PMPI_Info_get_valuelen(MPI_Info arg_0, char *arg_1, int *arg_2, int *arg_3);
_EXTERN_C_ int MPI_Info_get_valuelen(MPI_Info arg_0, char *arg_1, int *arg_2, int *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Info_get_valuelen(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Info_set ================== */
_EXTERN_C_ int PMPI_Info_set(MPI_Info arg_0, char *arg_1, char *arg_2);
_EXTERN_C_ int MPI_Info_set(MPI_Info arg_0, char *arg_1, char *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Info_set(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Get_processor_name ================== */
_EXTERN_C_ int PMPI_Get_processor_name(char *arg_0, int *arg_1);
_EXTERN_C_ int MPI_Get_processor_name(char *arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Get_processor_name(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_read_ordered_end ================== */
_EXTERN_C_ int PMPI_File_read_ordered_end(MPI_File arg_0, void *arg_1, MPI_Status *arg_2);
_EXTERN_C_ int MPI_File_read_ordered_end(MPI_File arg_0, void *arg_1, MPI_Status *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_read_ordered_end(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Accumulate ================== */
_EXTERN_C_ int PMPI_Accumulate(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, MPI_Aint arg_4, int arg_5, MPI_Datatype arg_6, MPI_Op arg_7, MPI_Win arg_8);
_EXTERN_C_ int MPI_Accumulate(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, MPI_Aint arg_4, int arg_5, MPI_Datatype arg_6, MPI_Op arg_7, MPI_Win arg_8) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Accumulate(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_set_attr ================== */
_EXTERN_C_ int PMPI_Type_set_attr(MPI_Datatype arg_0, int arg_1, void *arg_2);
_EXTERN_C_ int MPI_Type_set_attr(MPI_Datatype arg_0, int arg_1, void *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_set_attr(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Group_union ================== */
_EXTERN_C_ int PMPI_Group_union(MPI_Group arg_0, MPI_Group arg_1, MPI_Group *arg_2);
_EXTERN_C_ int MPI_Group_union(MPI_Group arg_0, MPI_Group arg_1, MPI_Group *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Group_union(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Alltoall ================== */
_EXTERN_C_ int PMPI_Alltoall(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int arg_4, MPI_Datatype arg_5, MPI_Comm arg_6);
_EXTERN_C_ int MPI_Alltoall(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int arg_4, MPI_Datatype arg_5, MPI_Comm arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_6);

   _wrap_py_return_val = PMPI_Alltoall(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Cart_sub ================== */
_EXTERN_C_ int PMPI_Cart_sub(MPI_Comm arg_0, int *arg_1, MPI_Comm *arg_2);
_EXTERN_C_ int MPI_Cart_sub(MPI_Comm arg_0, int *arg_1, MPI_Comm *arg_2) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Cart_sub(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Info_get ================== */
_EXTERN_C_ int PMPI_Info_get(MPI_Info arg_0, char *arg_1, int arg_2, char *arg_3, int *arg_4);
_EXTERN_C_ int MPI_Info_get(MPI_Info arg_0, char *arg_1, int arg_2, char *arg_3, int *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Info_get(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Start ================== */
_EXTERN_C_ int PMPI_Start(MPI_Request *arg_0);
_EXTERN_C_ int MPI_Start(MPI_Request *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Start(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_spawn ================== */
_EXTERN_C_ int PMPI_Comm_spawn(char *arg_0, char *arg_1[], int arg_2, MPI_Info arg_3, int arg_4, MPI_Comm arg_5, MPI_Comm *arg_6, int arg_7[]);
_EXTERN_C_ int MPI_Comm_spawn(char *arg_0, char *arg_1[], int arg_2, MPI_Info arg_3, int arg_4, MPI_Comm arg_5, MPI_Comm *arg_6, int arg_7[]) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Comm_spawn(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Group_intersection ================== */
_EXTERN_C_ int PMPI_Group_intersection(MPI_Group arg_0, MPI_Group arg_1, MPI_Group *arg_2);
_EXTERN_C_ int MPI_Group_intersection(MPI_Group arg_0, MPI_Group arg_1, MPI_Group *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Group_intersection(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_free_keyval ================== */
_EXTERN_C_ int PMPI_Type_free_keyval(int *arg_0);
_EXTERN_C_ int MPI_Type_free_keyval(int *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_free_keyval(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Reduce_scatter_block ================== */
_EXTERN_C_ int PMPI_Reduce_scatter_block(void *sendbuf, void *recvbuf, int recvcount, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm);
_EXTERN_C_ int MPI_Reduce_scatter_block(void *sendbuf, void *recvbuf, int recvcount, MPI_Datatype datatype, MPI_Op op, MPI_Comm comm) { 
    int _wrap_py_return_val = 0;
{
   swap_world(comm);

   _wrap_py_return_val = PMPI_Reduce_scatter_block(sendbuf, recvbuf, recvcount, datatype, op, comm);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_get_contents ================== */
_EXTERN_C_ int PMPI_Type_get_contents(MPI_Datatype arg_0, int arg_1, int arg_2, int arg_3, int arg_4[], MPI_Aint arg_5[], MPI_Datatype arg_6[]);
_EXTERN_C_ int MPI_Type_get_contents(MPI_Datatype arg_0, int arg_1, int arg_2, int arg_3, int arg_4[], MPI_Aint arg_5[], MPI_Datatype arg_6[]) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_get_contents(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_get_view ================== */
_EXTERN_C_ int PMPI_File_get_view(MPI_File arg_0, MPI_Offset *arg_1, MPI_Datatype *arg_2, MPI_Datatype *arg_3, char *arg_4);
_EXTERN_C_ int MPI_File_get_view(MPI_File arg_0, MPI_Offset *arg_1, MPI_Datatype *arg_2, MPI_Datatype *arg_3, char *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_get_view(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_free ================== */
_EXTERN_C_ int PMPI_Type_free(MPI_Datatype *arg_0);
_EXTERN_C_ int MPI_Type_free(MPI_Datatype *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_free(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_test ================== */
_EXTERN_C_ int PMPI_Win_test(MPI_Win arg_0, int *arg_1);
_EXTERN_C_ int MPI_Win_test(MPI_Win arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_test(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_write_at_all_begin ================== */
_EXTERN_C_ int PMPI_File_write_at_all_begin(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4);
_EXTERN_C_ int MPI_File_write_at_all_begin(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_write_at_all_begin(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Unpack_external ================== */
_EXTERN_C_ int PMPI_Unpack_external(char *arg_0, void *arg_1, MPI_Aint arg_2, MPI_Aint *arg_3, void *arg_4, int arg_5, MPI_Datatype arg_6);
_EXTERN_C_ int MPI_Unpack_external(char *arg_0, void *arg_1, MPI_Aint arg_2, MPI_Aint *arg_3, void *arg_4, int arg_5, MPI_Datatype arg_6) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Unpack_external(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Errhandler_set ================== */
_EXTERN_C_ int PMPI_Errhandler_set(MPI_Comm arg_0, MPI_Errhandler arg_1);
_EXTERN_C_ int MPI_Errhandler_set(MPI_Comm arg_0, MPI_Errhandler arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Errhandler_set(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_read_at_all_begin ================== */
_EXTERN_C_ int PMPI_File_read_at_all_begin(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4);
_EXTERN_C_ int MPI_File_read_at_all_begin(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_read_at_all_begin(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_get_errhandler ================== */
_EXTERN_C_ int PMPI_Comm_get_errhandler(MPI_Comm arg_0, MPI_Errhandler *arg_1);
_EXTERN_C_ int MPI_Comm_get_errhandler(MPI_Comm arg_0, MPI_Errhandler *arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_get_errhandler(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_seek_shared ================== */
_EXTERN_C_ int PMPI_File_seek_shared(MPI_File arg_0, MPI_Offset arg_1, int arg_2);
_EXTERN_C_ int MPI_File_seek_shared(MPI_File arg_0, MPI_Offset arg_1, int arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_seek_shared(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_lock ================== */
_EXTERN_C_ int PMPI_Win_lock(int arg_0, int arg_1, int arg_2, MPI_Win arg_3);
_EXTERN_C_ int MPI_Win_lock(int arg_0, int arg_1, int arg_2, MPI_Win arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_lock(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_create ================== */
_EXTERN_C_ int PMPI_Win_create(void *arg_0, MPI_Aint arg_1, int arg_2, MPI_Info arg_3, MPI_Comm arg_4, MPI_Win *arg_5);
_EXTERN_C_ int MPI_Win_create(void *arg_0, MPI_Aint arg_1, int arg_2, MPI_Info arg_3, MPI_Comm arg_4, MPI_Win *arg_5) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_4);

   _wrap_py_return_val = PMPI_Win_create(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Test ================== */
_EXTERN_C_ int PMPI_Test(MPI_Request *arg_0, int *arg_1, MPI_Status *arg_2);
_EXTERN_C_ int MPI_Test(MPI_Request *arg_0, int *arg_1, MPI_Status *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Test(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_write_all_end ================== */
_EXTERN_C_ int PMPI_File_write_all_end(MPI_File arg_0, void *arg_1, MPI_Status *arg_2);
_EXTERN_C_ int MPI_File_write_all_end(MPI_File arg_0, void *arg_1, MPI_Status *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_write_all_end(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Reduce_local ================== */
_EXTERN_C_ int PMPI_Reduce_local(void *inbuf, void *inoutbuf, int count, MPI_Datatype datatype, MPI_Op op);
_EXTERN_C_ int MPI_Reduce_local(void *inbuf, void *inoutbuf, int count, MPI_Datatype datatype, MPI_Op op) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Reduce_local(inbuf, inoutbuf, count, datatype, op);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_start ================== */
_EXTERN_C_ int PMPI_Win_start(MPI_Group arg_0, int arg_1, MPI_Win arg_2);
_EXTERN_C_ int MPI_Win_start(MPI_Group arg_0, int arg_1, MPI_Win arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_start(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_get_size ================== */
_EXTERN_C_ int PMPI_File_get_size(MPI_File arg_0, MPI_Offset *arg_1);
_EXTERN_C_ int MPI_File_get_size(MPI_File arg_0, MPI_Offset *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_get_size(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Finalized ================== */
_EXTERN_C_ int PMPI_Finalized(int *arg_0);
_EXTERN_C_ int MPI_Finalized(int *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Finalized(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_free_keyval ================== */
_EXTERN_C_ int PMPI_Win_free_keyval(int *arg_0);
_EXTERN_C_ int MPI_Win_free_keyval(int *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_free_keyval(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Waitany ================== */
_EXTERN_C_ int PMPI_Waitany(int arg_0, MPI_Request *arg_1, int *arg_2, MPI_Status *arg_3);
_EXTERN_C_ int MPI_Waitany(int arg_0, MPI_Request *arg_1, int *arg_2, MPI_Status *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Waitany(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_indexed ================== */
_EXTERN_C_ int PMPI_Type_indexed(int arg_0, int *arg_1, int *arg_2, MPI_Datatype arg_3, MPI_Datatype *arg_4);
_EXTERN_C_ int MPI_Type_indexed(int arg_0, int *arg_1, int *arg_2, MPI_Datatype arg_3, MPI_Datatype *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_indexed(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Op_commutative ================== */
_EXTERN_C_ int PMPI_Op_commutative(MPI_Op op, int *commute);
_EXTERN_C_ int MPI_Op_commutative(MPI_Op op, int *commute) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Op_commutative(op, commute);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Gather ================== */
_EXTERN_C_ int PMPI_Gather(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int arg_4, MPI_Datatype arg_5, int arg_6, MPI_Comm arg_7);
_EXTERN_C_ int MPI_Gather(void *arg_0, int arg_1, MPI_Datatype arg_2, void *arg_3, int arg_4, MPI_Datatype arg_5, int arg_6, MPI_Comm arg_7) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_7);

   _wrap_py_return_val = PMPI_Gather(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Get_elements ================== */
_EXTERN_C_ int PMPI_Get_elements(MPI_Status *arg_0, MPI_Datatype arg_1, int *arg_2);
_EXTERN_C_ int MPI_Get_elements(MPI_Status *arg_0, MPI_Datatype arg_1, int *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Get_elements(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_lb ================== */
_EXTERN_C_ int PMPI_Type_lb(MPI_Datatype arg_0, MPI_Aint *arg_1);
_EXTERN_C_ int MPI_Type_lb(MPI_Datatype arg_0, MPI_Aint *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_lb(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_read_at_all_end ================== */
_EXTERN_C_ int PMPI_File_read_at_all_end(MPI_File arg_0, void *arg_1, MPI_Status *arg_2);
_EXTERN_C_ int MPI_File_read_at_all_end(MPI_File arg_0, void *arg_1, MPI_Status *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_read_at_all_end(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_set_view ================== */
_EXTERN_C_ int PMPI_File_set_view(MPI_File arg_0, MPI_Offset arg_1, MPI_Datatype arg_2, MPI_Datatype arg_3, char *arg_4, MPI_Info arg_5);
_EXTERN_C_ int MPI_File_set_view(MPI_File arg_0, MPI_Offset arg_1, MPI_Datatype arg_2, MPI_Datatype arg_3, char *arg_4, MPI_Info arg_5) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_set_view(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Unpack ================== */
_EXTERN_C_ int PMPI_Unpack(void *arg_0, int arg_1, int *arg_2, void *arg_3, int arg_4, MPI_Datatype arg_5, MPI_Comm arg_6);
_EXTERN_C_ int MPI_Unpack(void *arg_0, int arg_1, int *arg_2, void *arg_3, int arg_4, MPI_Datatype arg_5, MPI_Comm arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_6);

   _wrap_py_return_val = PMPI_Unpack(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_ub ================== */
_EXTERN_C_ int PMPI_Type_ub(MPI_Datatype arg_0, MPI_Aint *arg_1);
_EXTERN_C_ int MPI_Type_ub(MPI_Datatype arg_0, MPI_Aint *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_ub(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Status_set_elements ================== */
_EXTERN_C_ int PMPI_Status_set_elements(MPI_Status *arg_0, MPI_Datatype arg_1, int arg_2);
_EXTERN_C_ int MPI_Status_set_elements(MPI_Status *arg_0, MPI_Datatype arg_1, int arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Status_set_elements(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_delete_attr ================== */
_EXTERN_C_ int PMPI_Win_delete_attr(MPI_Win arg_0, int arg_1);
_EXTERN_C_ int MPI_Win_delete_attr(MPI_Win arg_0, int arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_delete_attr(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Testany ================== */
_EXTERN_C_ int PMPI_Testany(int arg_0, MPI_Request *arg_1, int *arg_2, int *arg_3, MPI_Status *arg_4);
_EXTERN_C_ int MPI_Testany(int arg_0, MPI_Request *arg_1, int *arg_2, int *arg_3, MPI_Status *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Testany(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_set_atomicity ================== */
_EXTERN_C_ int PMPI_File_set_atomicity(MPI_File arg_0, int arg_1);
_EXTERN_C_ int MPI_File_set_atomicity(MPI_File arg_0, int arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_set_atomicity(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Group_range_incl ================== */
_EXTERN_C_ int PMPI_Group_range_incl(MPI_Group arg_0, int arg_1, int arg_2[][3], MPI_Group *arg_3);
_EXTERN_C_ int MPI_Group_range_incl(MPI_Group arg_0, int arg_1, int arg_2[][3], MPI_Group *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Group_range_incl(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Get ================== */
_EXTERN_C_ int PMPI_Get(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, MPI_Aint arg_4, int arg_5, MPI_Datatype arg_6, MPI_Win arg_7);
_EXTERN_C_ int MPI_Get(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, MPI_Aint arg_4, int arg_5, MPI_Datatype arg_6, MPI_Win arg_7) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Get(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_vector ================== */
_EXTERN_C_ int PMPI_Type_vector(int arg_0, int arg_1, int arg_2, MPI_Datatype arg_3, MPI_Datatype *arg_4);
_EXTERN_C_ int MPI_Type_vector(int arg_0, int arg_1, int arg_2, MPI_Datatype arg_3, MPI_Datatype *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_vector(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_write_at_all_end ================== */
_EXTERN_C_ int PMPI_File_write_at_all_end(MPI_File arg_0, void *arg_1, MPI_Status *arg_2);
_EXTERN_C_ int MPI_File_write_at_all_end(MPI_File arg_0, void *arg_1, MPI_Status *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_write_at_all_end(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_get_true_extent ================== */
_EXTERN_C_ int PMPI_Type_get_true_extent(MPI_Datatype arg_0, MPI_Aint *arg_1, MPI_Aint *arg_2);
_EXTERN_C_ int MPI_Type_get_true_extent(MPI_Datatype arg_0, MPI_Aint *arg_1, MPI_Aint *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_get_true_extent(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Graph_get ================== */
_EXTERN_C_ int PMPI_Graph_get(MPI_Comm arg_0, int arg_1, int arg_2, int *arg_3, int *arg_4);
_EXTERN_C_ int MPI_Graph_get(MPI_Comm arg_0, int arg_1, int arg_2, int *arg_3, int *arg_4) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Graph_get(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Info_delete ================== */
_EXTERN_C_ int PMPI_Info_delete(MPI_Info arg_0, char *arg_1);
_EXTERN_C_ int MPI_Info_delete(MPI_Info arg_0, char *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Info_delete(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Cart_rank ================== */
_EXTERN_C_ int PMPI_Cart_rank(MPI_Comm arg_0, int *arg_1, int *arg_2);
_EXTERN_C_ int MPI_Cart_rank(MPI_Comm arg_0, int *arg_1, int *arg_2) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Cart_rank(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_read_ordered ================== */
_EXTERN_C_ int PMPI_File_read_ordered(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4);
_EXTERN_C_ int MPI_File_read_ordered(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_read_ordered(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_set_name ================== */
_EXTERN_C_ int PMPI_Comm_set_name(MPI_Comm arg_0, char *arg_1);
_EXTERN_C_ int MPI_Comm_set_name(MPI_Comm arg_0, char *arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_set_name(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Pack_external_size ================== */
_EXTERN_C_ int PMPI_Pack_external_size(char *arg_0, int arg_1, MPI_Datatype arg_2, MPI_Aint *arg_3);
_EXTERN_C_ int MPI_Pack_external_size(char *arg_0, int arg_1, MPI_Datatype arg_2, MPI_Aint *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Pack_external_size(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_join ================== */
_EXTERN_C_ int PMPI_Comm_join(int arg_0, MPI_Comm *arg_1);
_EXTERN_C_ int MPI_Comm_join(int arg_0, MPI_Comm *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Comm_join(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_read_at ================== */
_EXTERN_C_ int PMPI_File_read_at(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4, MPI_Status *arg_5);
_EXTERN_C_ int MPI_File_read_at(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4, MPI_Status *arg_5) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_read_at(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Keyval_free ================== */
_EXTERN_C_ int PMPI_Keyval_free(int *arg_0);
_EXTERN_C_ int MPI_Keyval_free(int *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Keyval_free(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_wait ================== */
_EXTERN_C_ int PMPI_Win_wait(MPI_Win arg_0);
_EXTERN_C_ int MPI_Win_wait(MPI_Win arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_wait(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Alloc_mem ================== */
_EXTERN_C_ int PMPI_Alloc_mem(MPI_Aint arg_0, MPI_Info info, void *baseptr);
_EXTERN_C_ int MPI_Alloc_mem(MPI_Aint arg_0, MPI_Info info, void *baseptr) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Alloc_mem(arg_0, info, baseptr);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_iwrite_at ================== */
_EXTERN_C_ int PMPI_File_iwrite_at(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4, MPI_Request *arg_5);
_EXTERN_C_ int MPI_File_iwrite_at(MPI_File arg_0, MPI_Offset arg_1, void *arg_2, int arg_3, MPI_Datatype arg_4, MPI_Request *arg_5) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_iwrite_at(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_size ================== */
_EXTERN_C_ int PMPI_Type_size(MPI_Datatype arg_0, int *arg_1);
_EXTERN_C_ int MPI_Type_size(MPI_Datatype arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_size(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_preallocate ================== */
_EXTERN_C_ int PMPI_File_preallocate(MPI_File arg_0, MPI_Offset arg_1);
_EXTERN_C_ int MPI_File_preallocate(MPI_File arg_0, MPI_Offset arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_preallocate(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_get_atomicity ================== */
_EXTERN_C_ int PMPI_File_get_atomicity(MPI_File arg_0, int *arg_1);
_EXTERN_C_ int MPI_File_get_atomicity(MPI_File arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_get_atomicity(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Dist_graph_create ================== */
_EXTERN_C_ int PMPI_Dist_graph_create(MPI_Comm comm_old, int n, int arg_2[], int arg_3[], int arg_4[], int arg_5[], MPI_Info info, int reorder, MPI_Comm *comm_dist_graph);
_EXTERN_C_ int MPI_Dist_graph_create(MPI_Comm comm_old, int n, int arg_2[], int arg_3[], int arg_4[], int arg_5[], MPI_Info info, int reorder, MPI_Comm *comm_dist_graph) { 
    int _wrap_py_return_val = 0;
{
   swap_world(comm_old);

   _wrap_py_return_val = PMPI_Dist_graph_create(comm_old, n, arg_2, arg_3, arg_4, arg_5, info, reorder, comm_dist_graph);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_create_darray ================== */
_EXTERN_C_ int PMPI_Type_create_darray(int arg_0, int arg_1, int arg_2, int arg_3[], int arg_4[], int arg_5[], int arg_6[], int arg_7, MPI_Datatype arg_8, MPI_Datatype *arg_9);
_EXTERN_C_ int MPI_Type_create_darray(int arg_0, int arg_1, int arg_2, int arg_3[], int arg_4[], int arg_5[], int arg_6[], int arg_7, MPI_Datatype arg_8, MPI_Datatype *arg_9) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_create_darray(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8, arg_9);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_set_errhandler ================== */
_EXTERN_C_ int PMPI_Comm_set_errhandler(MPI_Comm arg_0, MPI_Errhandler arg_1);
_EXTERN_C_ int MPI_Comm_set_errhandler(MPI_Comm arg_0, MPI_Errhandler arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_set_errhandler(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_spawn_multiple ================== */
_EXTERN_C_ int PMPI_Comm_spawn_multiple(int arg_0, char *arg_1[], char **arg_2[], int arg_3[], MPI_Info arg_4[], int arg_5, MPI_Comm arg_6, MPI_Comm *arg_7, int arg_8[]);
_EXTERN_C_ int MPI_Comm_spawn_multiple(int arg_0, char *arg_1[], char **arg_2[], int arg_3[], MPI_Info arg_4[], int arg_5, MPI_Comm arg_6, MPI_Comm *arg_7, int arg_8[]) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_6);

   _wrap_py_return_val = PMPI_Comm_spawn_multiple(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Probe ================== */
_EXTERN_C_ int PMPI_Probe(int arg_0, int arg_1, MPI_Comm arg_2, MPI_Status *arg_3);
_EXTERN_C_ int MPI_Probe(int arg_0, int arg_1, MPI_Comm arg_2, MPI_Status *arg_3) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_2);

   _wrap_py_return_val = PMPI_Probe(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_accept ================== */
_EXTERN_C_ int PMPI_Comm_accept(char *arg_0, MPI_Info arg_1, int arg_2, MPI_Comm arg_3, MPI_Comm *arg_4);
_EXTERN_C_ int MPI_Comm_accept(char *arg_0, MPI_Info arg_1, int arg_2, MPI_Comm arg_3, MPI_Comm *arg_4) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_3);

   _wrap_py_return_val = PMPI_Comm_accept(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_create_struct ================== */
_EXTERN_C_ int PMPI_Type_create_struct(int arg_0, int arg_1[], MPI_Aint arg_2[], MPI_Datatype arg_3[], MPI_Datatype *arg_4);
_EXTERN_C_ int MPI_Type_create_struct(int arg_0, int arg_1[], MPI_Aint arg_2[], MPI_Datatype arg_3[], MPI_Datatype *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_create_struct(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Wtick ================== */
_EXTERN_C_ double PMPI_Wtick();
_EXTERN_C_ double MPI_Wtick() { 
    double _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Wtick();
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_set_info ================== */
_EXTERN_C_ int PMPI_File_set_info(MPI_File arg_0, MPI_Info arg_1);
_EXTERN_C_ int MPI_File_set_info(MPI_File arg_0, MPI_Info arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_set_info(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Cart_map ================== */
_EXTERN_C_ int PMPI_Cart_map(MPI_Comm arg_0, int arg_1, int *arg_2, int *arg_3, int *arg_4);
_EXTERN_C_ int MPI_Cart_map(MPI_Comm arg_0, int arg_1, int *arg_2, int *arg_3, int *arg_4) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Cart_map(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_hvector ================== */
_EXTERN_C_ int PMPI_Type_hvector(int arg_0, int arg_1, MPI_Aint arg_2, MPI_Datatype arg_3, MPI_Datatype *arg_4);
_EXTERN_C_ int MPI_Type_hvector(int arg_0, int arg_1, MPI_Aint arg_2, MPI_Datatype arg_3, MPI_Datatype *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_hvector(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_seek ================== */
_EXTERN_C_ int PMPI_File_seek(MPI_File arg_0, MPI_Offset arg_1, int arg_2);
_EXTERN_C_ int MPI_File_seek(MPI_File arg_0, MPI_Offset arg_1, int arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_seek(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Dist_graph_neighbors_count ================== */
_EXTERN_C_ int PMPI_Dist_graph_neighbors_count(MPI_Comm comm, int *indegree, int *outdegree, int *weighted);
_EXTERN_C_ int MPI_Dist_graph_neighbors_count(MPI_Comm comm, int *indegree, int *outdegree, int *weighted) { 
    int _wrap_py_return_val = 0;
{
   swap_world(comm);

   _wrap_py_return_val = PMPI_Dist_graph_neighbors_count(comm, indegree, outdegree, weighted);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_close ================== */
_EXTERN_C_ int PMPI_File_close(MPI_File *arg_0);
_EXTERN_C_ int MPI_File_close(MPI_File *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_close(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_create_subarray ================== */
_EXTERN_C_ int PMPI_Type_create_subarray(int arg_0, int arg_1[], int arg_2[], int arg_3[], int arg_4, MPI_Datatype arg_5, MPI_Datatype *arg_6);
_EXTERN_C_ int MPI_Type_create_subarray(int arg_0, int arg_1[], int arg_2[], int arg_3[], int arg_4, MPI_Datatype arg_5, MPI_Datatype *arg_6) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_create_subarray(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Unpublish_name ================== */
_EXTERN_C_ int PMPI_Unpublish_name(char *arg_0, MPI_Info arg_1, char *arg_2);
_EXTERN_C_ int MPI_Unpublish_name(char *arg_0, MPI_Info arg_1, char *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Unpublish_name(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_get_byte_offset ================== */
_EXTERN_C_ int PMPI_File_get_byte_offset(MPI_File arg_0, MPI_Offset arg_1, MPI_Offset *arg_2);
_EXTERN_C_ int MPI_File_get_byte_offset(MPI_File arg_0, MPI_Offset arg_1, MPI_Offset *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_get_byte_offset(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_read_all ================== */
_EXTERN_C_ int PMPI_File_read_all(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4);
_EXTERN_C_ int MPI_File_read_all(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_read_all(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_call_errhandler ================== */
_EXTERN_C_ int PMPI_Comm_call_errhandler(MPI_Comm arg_0, int arg_1);
_EXTERN_C_ int MPI_Comm_call_errhandler(MPI_Comm arg_0, int arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_call_errhandler(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_dup ================== */
_EXTERN_C_ int PMPI_Comm_dup(MPI_Comm arg_0, MPI_Comm *arg_1);
_EXTERN_C_ int MPI_Comm_dup(MPI_Comm arg_0, MPI_Comm *arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_dup(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Waitall ================== */
_EXTERN_C_ int PMPI_Waitall(int arg_0, MPI_Request *arg_1, MPI_Status *arg_2);
_EXTERN_C_ int MPI_Waitall(int arg_0, MPI_Request *arg_1, MPI_Status *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Waitall(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Topo_test ================== */
_EXTERN_C_ int PMPI_Topo_test(MPI_Comm arg_0, int *arg_1);
_EXTERN_C_ int MPI_Topo_test(MPI_Comm arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Topo_test(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Finalize ================== */
_EXTERN_C_ int PMPI_Finalize();
_EXTERN_C_ int MPI_Finalize() { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Finalize();
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Ssend ================== */
_EXTERN_C_ int PMPI_Ssend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5);
_EXTERN_C_ int MPI_Ssend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Ssend(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Group_size ================== */
_EXTERN_C_ int PMPI_Group_size(MPI_Group arg_0, int *arg_1);
_EXTERN_C_ int MPI_Group_size(MPI_Group arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Group_size(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Testall ================== */
_EXTERN_C_ int PMPI_Testall(int arg_0, MPI_Request *arg_1, int *arg_2, MPI_Status *arg_3);
_EXTERN_C_ int MPI_Testall(int arg_0, MPI_Request *arg_1, int *arg_2, MPI_Status *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Testall(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Attr_put ================== */
_EXTERN_C_ int PMPI_Attr_put(MPI_Comm arg_0, int arg_1, void *arg_2);
_EXTERN_C_ int MPI_Attr_put(MPI_Comm arg_0, int arg_1, void *arg_2) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Attr_put(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Barrier ================== */
_EXTERN_C_ int PMPI_Barrier(MPI_Comm arg_0);
_EXTERN_C_ int MPI_Barrier(MPI_Comm arg_0) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Barrier(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_free ================== */
_EXTERN_C_ int PMPI_Win_free(MPI_Win *arg_0);
_EXTERN_C_ int MPI_Win_free(MPI_Win *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_free(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Info_get_nthkey ================== */
_EXTERN_C_ int PMPI_Info_get_nthkey(MPI_Info arg_0, int arg_1, char *arg_2);
_EXTERN_C_ int MPI_Info_get_nthkey(MPI_Info arg_0, int arg_1, char *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Info_get_nthkey(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Alltoallw ================== */
_EXTERN_C_ int PMPI_Alltoallw(void *arg_0, int arg_1[], int arg_2[], MPI_Datatype arg_3[], void *arg_4, int arg_5[], int arg_6[], MPI_Datatype arg_7[], MPI_Comm arg_8);
_EXTERN_C_ int MPI_Alltoallw(void *arg_0, int arg_1[], int arg_2[], MPI_Datatype arg_3[], void *arg_4, int arg_5[], int arg_6[], MPI_Datatype arg_7[], MPI_Comm arg_8) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_8);

   _wrap_py_return_val = PMPI_Alltoallw(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Alltoallv ================== */
_EXTERN_C_ int PMPI_Alltoallv(void *arg_0, int *arg_1, int *arg_2, MPI_Datatype arg_3, void *arg_4, int *arg_5, int *arg_6, MPI_Datatype arg_7, MPI_Comm arg_8);
_EXTERN_C_ int MPI_Alltoallv(void *arg_0, int *arg_1, int *arg_2, MPI_Datatype arg_3, void *arg_4, int *arg_5, int *arg_6, MPI_Datatype arg_7, MPI_Comm arg_8) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_8);

   _wrap_py_return_val = PMPI_Alltoallv(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6, arg_7, arg_8);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Bsend_init ================== */
_EXTERN_C_ int PMPI_Bsend_init(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6);
_EXTERN_C_ int MPI_Bsend_init(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Bsend_init(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Close_port ================== */
_EXTERN_C_ int PMPI_Close_port(char *arg_0);
_EXTERN_C_ int MPI_Close_port(char *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Close_port(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Op_free ================== */
_EXTERN_C_ int PMPI_Op_free(MPI_Op *arg_0);
_EXTERN_C_ int MPI_Op_free(MPI_Op *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Op_free(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_set_errhandler ================== */
_EXTERN_C_ int PMPI_Win_set_errhandler(MPI_Win arg_0, MPI_Errhandler arg_1);
_EXTERN_C_ int MPI_Win_set_errhandler(MPI_Win arg_0, MPI_Errhandler arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_set_errhandler(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Isend ================== */
_EXTERN_C_ int PMPI_Isend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6);
_EXTERN_C_ int MPI_Isend(void *arg_0, int arg_1, MPI_Datatype arg_2, int arg_3, int arg_4, MPI_Comm arg_5, MPI_Request *arg_6) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_5);

   _wrap_py_return_val = PMPI_Isend(arg_0, arg_1, arg_2, arg_3, arg_4, arg_5, arg_6);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_get_name ================== */
_EXTERN_C_ int PMPI_Comm_get_name(MPI_Comm arg_0, char *arg_1, int *arg_2);
_EXTERN_C_ int MPI_Comm_get_name(MPI_Comm arg_0, char *arg_1, int *arg_2) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_get_name(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Buffer_attach ================== */
_EXTERN_C_ int PMPI_Buffer_attach(void *arg_0, int arg_1);
_EXTERN_C_ int MPI_Buffer_attach(void *arg_0, int arg_1) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Buffer_attach(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_write_ordered ================== */
_EXTERN_C_ int PMPI_File_write_ordered(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4);
_EXTERN_C_ int MPI_File_write_ordered(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3, MPI_Status *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_write_ordered(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Group_range_excl ================== */
_EXTERN_C_ int PMPI_Group_range_excl(MPI_Group arg_0, int arg_1, int arg_2[][3], MPI_Group *arg_3);
_EXTERN_C_ int MPI_Group_range_excl(MPI_Group arg_0, int arg_1, int arg_2[][3], MPI_Group *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Group_range_excl(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_split ================== */
_EXTERN_C_ int PMPI_Comm_split(MPI_Comm arg_0, int arg_1, int arg_2, MPI_Comm *arg_3);
_EXTERN_C_ int MPI_Comm_split(MPI_Comm arg_0, int arg_1, int arg_2, MPI_Comm *arg_3) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_split(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Keyval_create ================== */
_EXTERN_C_ int PMPI_Keyval_create(MPI_Copy_function *arg_0, MPI_Delete_function *arg_1, int *arg_2, void *arg_3);
_EXTERN_C_ int MPI_Keyval_create(MPI_Copy_function *arg_0, MPI_Delete_function *arg_1, int *arg_2, void *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Keyval_create(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Request_get_status ================== */
_EXTERN_C_ int PMPI_Request_get_status(MPI_Request arg_0, int *arg_1, MPI_Status *arg_2);
_EXTERN_C_ int MPI_Request_get_status(MPI_Request arg_0, int *arg_1, MPI_Status *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Request_get_status(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Group_free ================== */
_EXTERN_C_ int PMPI_Group_free(MPI_Group *arg_0);
_EXTERN_C_ int MPI_Group_free(MPI_Group *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Group_free(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_create_keyval ================== */
_EXTERN_C_ int PMPI_Type_create_keyval(MPI_Type_copy_attr_function *arg_0, MPI_Type_delete_attr_function *arg_1, int *arg_2, void *arg_3);
_EXTERN_C_ int MPI_Type_create_keyval(MPI_Type_copy_attr_function *arg_0, MPI_Type_delete_attr_function *arg_1, int *arg_2, void *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_create_keyval(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_write_ordered_begin ================== */
_EXTERN_C_ int PMPI_File_write_ordered_begin(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3);
_EXTERN_C_ int MPI_File_write_ordered_begin(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_write_ordered_begin(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_File_read_ordered_begin ================== */
_EXTERN_C_ int PMPI_File_read_ordered_begin(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3);
_EXTERN_C_ int MPI_File_read_ordered_begin(MPI_File arg_0, void *arg_1, int arg_2, MPI_Datatype arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_File_read_ordered_begin(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Iprobe ================== */
_EXTERN_C_ int PMPI_Iprobe(int arg_0, int arg_1, MPI_Comm arg_2, int *arg_3, MPI_Status *arg_4);
_EXTERN_C_ int MPI_Iprobe(int arg_0, int arg_1, MPI_Comm arg_2, int *arg_3, MPI_Status *arg_4) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_2);

   _wrap_py_return_val = PMPI_Iprobe(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Comm_rank ================== */
_EXTERN_C_ int PMPI_Comm_rank(MPI_Comm arg_0, int *arg_1);
_EXTERN_C_ int MPI_Comm_rank(MPI_Comm arg_0, int *arg_1) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Comm_rank(arg_0, arg_1);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Cancel ================== */
_EXTERN_C_ int PMPI_Cancel(MPI_Request *arg_0);
_EXTERN_C_ int MPI_Cancel(MPI_Request *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Cancel(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Register_datarep ================== */
_EXTERN_C_ int PMPI_Register_datarep(char *arg_0, MPI_Datarep_conversion_function *arg_1, MPI_Datarep_conversion_function *arg_2, MPI_Datarep_extent_function *arg_3, void *arg_4);
_EXTERN_C_ int MPI_Register_datarep(char *arg_0, MPI_Datarep_conversion_function *arg_1, MPI_Datarep_conversion_function *arg_2, MPI_Datarep_extent_function *arg_3, void *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Register_datarep(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Win_create_keyval ================== */
_EXTERN_C_ int PMPI_Win_create_keyval(MPI_Win_copy_attr_function *arg_0, MPI_Win_delete_attr_function *arg_1, int *arg_2, void *arg_3);
_EXTERN_C_ int MPI_Win_create_keyval(MPI_Win_copy_attr_function *arg_0, MPI_Win_delete_attr_function *arg_1, int *arg_2, void *arg_3) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Win_create_keyval(arg_0, arg_1, arg_2, arg_3);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Errhandler_free ================== */
_EXTERN_C_ int PMPI_Errhandler_free(MPI_Errhandler *arg_0);
_EXTERN_C_ int MPI_Errhandler_free(MPI_Errhandler *arg_0) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Errhandler_free(arg_0);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Dist_graph_create_adjacent ================== */
_EXTERN_C_ int PMPI_Dist_graph_create_adjacent(MPI_Comm comm_old, int indegree, int arg_2[], int arg_3[], int outdegree, int arg_5[], int arg_6[], MPI_Info info, int reorder, MPI_Comm *comm_dist_graph);
_EXTERN_C_ int MPI_Dist_graph_create_adjacent(MPI_Comm comm_old, int indegree, int arg_2[], int arg_3[], int outdegree, int arg_5[], int arg_6[], MPI_Info info, int reorder, MPI_Comm *comm_dist_graph) { 
    int _wrap_py_return_val = 0;
{
   swap_world(comm_old);

   _wrap_py_return_val = PMPI_Dist_graph_create_adjacent(comm_old, indegree, arg_2, arg_3, outdegree, arg_5, arg_6, info, reorder, comm_dist_graph);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_create_hindexed ================== */
_EXTERN_C_ int PMPI_Type_create_hindexed(int arg_0, int arg_1[], MPI_Aint arg_2[], MPI_Datatype arg_3, MPI_Datatype *arg_4);
_EXTERN_C_ int MPI_Type_create_hindexed(int arg_0, int arg_1[], MPI_Aint arg_2[], MPI_Datatype arg_3, MPI_Datatype *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_create_hindexed(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_match_size ================== */
_EXTERN_C_ int PMPI_Type_match_size(int arg_0, int arg_1, MPI_Datatype *arg_2);
_EXTERN_C_ int MPI_Type_match_size(int arg_0, int arg_1, MPI_Datatype *arg_2) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_match_size(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Graphdims_get ================== */
_EXTERN_C_ int PMPI_Graphdims_get(MPI_Comm arg_0, int *arg_1, int *arg_2);
_EXTERN_C_ int MPI_Graphdims_get(MPI_Comm arg_0, int *arg_1, int *arg_2) { 
    int _wrap_py_return_val = 0;
{
   swap_world(arg_0);

   _wrap_py_return_val = PMPI_Graphdims_get(arg_0, arg_1, arg_2);
}    return _wrap_py_return_val;
}

/* ================== C Wrappers for MPI_Type_create_hvector ================== */
_EXTERN_C_ int PMPI_Type_create_hvector(int arg_0, int arg_1, MPI_Aint arg_2, MPI_Datatype arg_3, MPI_Datatype *arg_4);
_EXTERN_C_ int MPI_Type_create_hvector(int arg_0, int arg_1, MPI_Aint arg_2, MPI_Datatype arg_3, MPI_Datatype *arg_4) { 
    int _wrap_py_return_val = 0;
{
   
   _wrap_py_return_val = PMPI_Type_create_hvector(arg_0, arg_1, arg_2, arg_3, arg_4);
}    return _wrap_py_return_val;
}
