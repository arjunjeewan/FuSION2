package fusion.frontend.runner;

import java.lang.management.ManagementFactory;
import java.lang.management.OperatingSystemMXBean;
import java.nio.file.Path;
import java.time.Clock;
import java.time.Duration;
import java.util.UUID;
import java.util.function.Supplier;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.convert.DurationStyle;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

import fusion.frontend.accounts.AccountService;
import fusion.frontend.pipeline.BatchGenerator;
import fusion.frontend.pipeline.DefaultPipeline;
import fusion.frontend.pipeline.FrontendVersion;
import fusion.frontend.pipeline.PipelineValidator;
import fusion.frontend.sessions.SessionStore;

/**
 * The runner's services (10.C11) and its settings (section 10, Settings): Spring properties, which
 * can also be set as environment variables. None of them changes what a run computes.
 */
@Configuration(proxyBeanMethods = false)
public class RunnerConfiguration {
    @Bean
    public ProcessBackendClient backendClient(@Value("${fusion.backend.executable:}") String executable,
                                              @Value("${fusion.backend.call-timeout:30s}") String callTimeout,
                                              @Value("${fusion.backend.max-calls:4}") int maxCalls) {
        return new ProcessBackendClient(Path.of(executable), DurationStyle.detectAndParse(callTimeout), maxCalls);
    }

    @Bean
    public DefaultPipeline defaultPipeline(BackendClient backend) {
        return new StandardPipelineLoader(() -> StandardPipelineLoader.homeOf(backend));
    }

    @Bean
    public GeneLookup geneLookup(BackendClient backend, @Value("${fusion.data-dir}") Path dataDir,
                                 @Value("${fusion.backend.call-timeout:30s}") String callTimeout) {
        return new GeneLookup(backend, DurationStyle.detectAndParse(callTimeout), dataDir);
    }

    @Bean
    public ExpressionQueries expressionQueries(BackendClient backend, @Value("${fusion.data-dir}") Path dataDir,
                                               @Value("${fusion.backend.call-timeout:30s}") String callTimeout,
                                               GeneLookup genes, SessionStore sessions) {
        return new ExpressionQueries(backend, DurationStyle.detectAndParse(callTimeout), dataDir, genes, sessions);
    }

    @Bean(destroyMethod = "close")
    public DefaultRunService runService(@Value("${fusion.data-dir}") Path dataDir,
                                        @Value("${fusion.results-dir}") Path resultsDir,
                                        @Value("${fusion.backend.executable:}") String executable,
                                        @Value("${fusion.runs.max-cores:0}") int maxCores,
                                        @Value("${fusion.runs.poll-interval:250ms}") String pollInterval,
                                        @Value("${fusion.runs.stop-grace:60s}") String stopGrace,
                                        BackendClient backend, SessionStore sessions, AccountService accounts,
                                        DefaultPipeline pipelines, PipelineValidator validator,
                                        BatchGenerator generator, GeneLookup genes, Clock clock,
                                        Supplier<UUID> ids) {
        int cores = maxCores >= 1 ? maxCores
                : RunQueue.defaultLimit(Runtime.getRuntime().availableProcessors(), physicalMemory());
        Duration poll = DurationStyle.detectAndParse(pollInterval);
        Duration grace = DurationStyle.detectAndParse(stopGrace);
        return new DefaultRunService(new DefaultRunService.Settings(dataDir, resultsDir, Path.of(executable), cores,
                poll, grace, FrontendVersion.current()), backend, sessions, accounts, pipelines, validator, generator,
                genes, clock, ids);
    }

    @Bean
    public ResultsIndex resultsIndex(DefaultRunService runs) {
        return new FileResultsIndex(runs::record);
    }

    /** Physical memory, for 10.C20's limit; unknown, the limit follows the CPUs alone. */
    static long physicalMemory() {
        OperatingSystemMXBean os = ManagementFactory.getOperatingSystemMXBean();
        if (os instanceof com.sun.management.OperatingSystemMXBean sun) {
            return sun.getTotalMemorySize();
        }
        return Long.MAX_VALUE;
    }
}
