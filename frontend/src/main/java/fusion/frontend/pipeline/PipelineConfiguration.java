package fusion.frontend.pipeline;

import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;

/** The pipeline module's services (10.C11): pure, and free of JavaFX. */
@Configuration(proxyBeanMethods = false)
public class PipelineConfiguration {
    @Bean
    public PipelineValidator pipelineValidator() {
        return new StandardPipelineValidator();
    }

    @Bean
    public BatchGenerator batchGenerator() {
        return new StandardBatchGenerator();
    }
}
