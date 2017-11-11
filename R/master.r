#' Master controlling the workers
#'
#' exchanging messages between the master and workers works the following way:
#'  * we have submitted a job where we don't know when it will start up
#'  * it starts, sends is a message list(id=0) indicating it is ready
#'  * we send it the function definition and common data
#'    * we also send it the first data set to work on
#'  * when we get any id > 0, it is a result that we store
#'    * and send the next data set/index to work on
#'  * when computatons are complete, we send id=0 to the worker
#'    * it responds with id=-1 (and usage stats) and shuts down
#'
#' @param qsys           Instance of QSys object
#' @param iter           Objects to be iterated in each function call
#' @param fail_on_error  If an error occurs on the workers, continue or fail?
#' @param wait_time      Time to wait between messages; set 0 for short calls
#'                       defaults to 1/sqrt(number_of_functon_calls)
#' @param chunk_size     Number of function calls to chunk together
#'                       defaults to 100 chunks per worker or max. 500 kb per chunk
#' @return               A list of whatever `fun` returned
master = function(qsys, iter, fail_on_error=TRUE, wait_time=NA, chunk_size=NA) {
    # prepare empty variables for managing results
    n_calls = nrow(iter)
    job_result = rep(list(NULL), n_calls)
    submit_index = 1:chunk_size
    jobs_running = list()
    warnings = list()
    shutdown = FALSE
    pkgver = utils::packageVersion("clustermq")
    pkg_warn = TRUE

    message("Running ", format(n_calls, big.mark=",", scientific=FALSE),
            " calculations (", chunk_size, " calls/chunk) ...")
    pb = utils::txtProgressBar(min=0, max=n_calls, style=3)

    # main event loop
    while((!shutdown && submit_index[1] <= n_calls) || qsys$workers_running > 0) {
        # wait for results only longer if we don't have all data yet
        if ((!shutdown && submit_index[1] <= n_calls) || length(jobs_running) > 0)
            msg = qsys$receive_data()
        else {
            msg = qsys$receive_data(timeout=10)
            if (is.null(msg)) {
                warning(sprintf("%i/%i workers did not shut down properly",
                        qsys$workers_running, qsys$workers), immediate.=TRUE)
                break
            }
        }

        switch(msg$id,
            "WORKER_UP" = {
                if (msg$pkgver != pkgver && pkg_warn) {
                    warning("\nVersion mismatch: master has ", pkgver,
                            ", worker ", msg$pkgver, immediate.=TRUE)
                    pkg_warn = FALSE
                }
                qsys$send_common_data()
            },
            "WORKER_READY" = {
                # process the result data if we got some
                if (!is.null(msg$result)) {
                    call_id = names(msg$result)
                    jobs_running[call_id] = NULL
                    job_result[as.integer(call_id)] = msg$result
                    utils::setTxtProgressBar(pb, submit_index[1] -
                                             length(jobs_running) - 1)

                    errors = sapply(msg$result, class) == "error"
                    if (any(errors) && fail_on_error == TRUE)
                        shutdown = TRUE
                    warnings = c(warnings, msg$warnings)
                }

                if (!shutdown && msg$token != qsys$data_token) {
                    qsys$send_common_data()

                } else if (!shutdown && submit_index[1] <= n_calls) {
                    # if we have work, send it to the worker
                    submit_index = submit_index[submit_index <= n_calls]
                    qsys$send_job_data(chunk = iter[submit_index, , drop=FALSE])
                    jobs_running[sprintf("%i", submit_index)] = TRUE
                    submit_index = submit_index + chunk_size

                    # adapt chunk size towards end of processing
                    cs = ceiling((n_calls - submit_index[1]) / qsys$workers_running)
                    if (cs < chunk_size) {
                        chunk_size = max(cs, 1)
                        submit_index = submit_index[1:chunk_size]
                    }

                } else if (!shutdown && qsys$reusable) {
                    qsys$send_wait()
                    if (length(jobs_running) == 0)
                        break

                } else # or else shut it down
                    qsys$send_shutdown_worker()
            },
            "WORKER_DONE" = {
                qsys$disconnect_worker(msg)
            },
            "WORKER_ERROR" = {
                stop("\nWorker error: ", msg$msg)
            }
        )

        Sys.sleep(wait_time)
    }

    close(pb)

    unravel_result(list(result=job_result, warnings=warnings),
                   at = min(submit_index)-1,
                   fail_on_error = fail_on_error)
}