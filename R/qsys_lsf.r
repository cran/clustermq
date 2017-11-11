#' LSF scheduler functions
#'
#' Derives from QSys to provide LSF-specific functions
LSF = R6::R6Class("LSF",
    inherit = QSys,

    public = list(
        initialize = function(...) {
            super$initialize(...)
        },

        submit_jobs = function(n_jobs, template=list(), log_worker=FALSE) {
            template$n_jobs = n_jobs
            filled = fill_template(template=LSF$template, master=private$master,
                                   values=template, log_worker=log_worker)

            success = system("bsub", input=filled, ignore.stdout=TRUE)
            if (success != 0) {
                print(filled)
                stop("Job submission failed with error code ", success)
            }
        },

        cleanup = function() {
            super$cleanup()
            dirty = self$workers_running > 0
            system(paste("bkill", private$job_id),
                   ignore.stdout=!dirty, ignore.stderr=!dirty)
        }
    ),

    private = list(
        job_id = NULL
    )
)

# Static method, process scheduler options and return updated object
LSF$setup = function() {
    user_template = getOption("clustermq.template.lsf")
    if (!is.null(user_template))
        LSF$template = readChar(user_template, file.info(user_template)$size)
    LSF
}

# Static method, overwritten in qsys w/ user option
LSF$template = paste(sep="\n",
    "#BSUB-J {{ job_name }}[1-{{ n_jobs }}]    # name of the job / array jobs",
    "#BSUB-g {{ job_group | /rzmq }}           # group the job belongs to",
    "#BSUB-o {{ log_file | /dev/null }}        # stdout + stderr",
    "#BSUB-M {{ memory | 4096 }}               # Memory requirements in Mbytes",
    "#BSUB-R rusage[mem={{ memory | 4096  }}]  # Memory requirements in Mbytes",
    "",
    "ulimit -v $(( 1024 * {{ memory | 4096 }} ))",
    "R --no-save --no-restore -e 'clustermq:::worker(\"{{ master }}\"')")