package com.starflash.rescue.api;

import com.starflash.rescue.model.AiChatRequest;
import com.starflash.rescue.model.AiResponse;
import com.starflash.rescue.service.ZhipuAiService;
import jakarta.validation.Valid;
import org.springframework.http.MediaType;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;
import reactor.core.publisher.Flux;

import java.util.Map;

@RestController
@RequestMapping("/api/ai")
public class AiController {
    private final ZhipuAiService zhipuAiService;

    public AiController(ZhipuAiService zhipuAiService) {
        this.zhipuAiService = zhipuAiService;
    }

    @PostMapping("/chat")
    public AiResponse chat(@Valid @RequestBody AiChatRequest request) {
        return zhipuAiService.chat(request);
    }

    @PostMapping(value = "/chat/stream", produces = MediaType.TEXT_EVENT_STREAM_VALUE)
    public Flux<String> chatStream(@Valid @RequestBody AiChatRequest request) {
        return zhipuAiService.streamChat(request);
    }

    @PostMapping("/injury-analysis")
    public AiResponse injuryAnalysis(@RequestBody Map<String, Object> body) {
        return zhipuAiService.chat(promptRequest("伤病智能分析", "请给出处置优先级、主要风险、现场急救建议和转运注意事项。", body));
    }

    @PostMapping("/route-optimize")
    public AiResponse routeOptimize(@RequestBody Map<String, Object> body) {
        return zhipuAiService.chat(promptRequest("救援路径优化", "请给出推荐路线、避险要点、进入条件和备选方案。", body));
    }

    @PostMapping("/vitals-interpret")
    public AiResponse vitalsInterpret(@RequestBody Map<String, Object> body) {
        return zhipuAiService.chat(promptRequest("生命体征解读", "请判断生命体征趋势风险，并给出持续监测和现场处置建议。", body));
    }

    @PostMapping("/environment-assessment")
    public AiResponse environmentAssessment(@RequestBody Map<String, Object> body) {
        return zhipuAiService.chat(promptRequest("环境风险评估", "请评估温湿度、气体和氧气风险，给出防护建议和撤离触发条件。", body));
    }

    @PostMapping("/generate-report")
    public AiResponse generateReport(@RequestBody Map<String, Object> body) {
        return zhipuAiService.chat(promptRequest("综合灾情报告", "请生成简明报告，包含态势摘要、救援优先级、资源调度和下一步行动。", body));
    }

    private AiChatRequest promptRequest(String title, String instruction, Map<String, Object> body) {
        return new AiChatRequest("任务：" + title + "\n要求：" + instruction + "\n数据：" + body, null);
    }
}
