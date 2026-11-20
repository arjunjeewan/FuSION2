package fusion.frontend.ui;

import org.springframework.context.ApplicationContext;

import fusion.frontend.accounts.AccountService;
import fusion.frontend.pipeline.BatchGenerator;
import fusion.frontend.pipeline.DefaultPipeline;
import fusion.frontend.pipeline.PipelineValidator;
import fusion.frontend.runner.BackendClient;
import fusion.frontend.runner.DefaultRunService;
import fusion.frontend.runner.ExpressionQueries;
import fusion.frontend.runner.GeneLookup;
import fusion.frontend.runner.ResultsIndex;
import fusion.frontend.sessions.SessionChecks;
import fusion.frontend.sessions.SessionStore;

/** What the screens call, in the same process (10.C5): the four service modules, and the platform. */
record Services(AccountService accounts, SessionStore sessions, SessionChecks checks, DefaultPipeline pipelines,
                PipelineValidator validator, BatchGenerator generator, DefaultRunService runs, ResultsIndex results,
                GeneLookup genes, ExpressionQueries expression, BackendClient backend, FxDispatch fx,
                PageOpener pages, FileDialogs dialogs) {
    static Services from(ApplicationContext c, FxDispatch fx, PageOpener pages, FileDialogs dialogs) {
        return new Services(c.getBean(AccountService.class), c.getBean(SessionStore.class),
                c.getBean(SessionChecks.class), c.getBean(DefaultPipeline.class), c.getBean(PipelineValidator.class),
                c.getBean(BatchGenerator.class), c.getBean(DefaultRunService.class), c.getBean(ResultsIndex.class),
                c.getBean(GeneLookup.class), c.getBean(ExpressionQueries.class), c.getBean(BackendClient.class), fx,
                pages, dialogs);
    }
}
