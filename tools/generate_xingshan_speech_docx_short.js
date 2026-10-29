const fs = require('fs');
const {
  Document,
  Packer,
  Paragraph,
  TextRun,
  HeadingLevel,
  AlignmentType,
  PageBreak,
  Footer,
  PageNumber,
} = require('docx');

const outputPath = 'D:\\table\\星闪智救_物联网比赛演讲稿_精简版_修改版.docx';
const font = 'Microsoft YaHei';

function run(text, options = {}) {
  return new TextRun({
    text,
    font,
    size: options.size ?? 24,
    bold: options.bold ?? false,
    color: options.color ?? '1F2937',
  });
}

function para(text, options = {}) {
  return new Paragraph({
    children: [run(text, options)],
    spacing: { before: options.before ?? 80, after: options.after ?? 80, line: 360 },
    alignment: options.align ?? AlignmentType.LEFT,
  });
}

function heading(text) {
  return new Paragraph({
    heading: HeadingLevel.HEADING_1,
    children: [run(text, { bold: true, size: 32, color: '0F172A' })],
    spacing: { before: 160, after: 160 },
  });
}

function pageBreak() {
  return new Paragraph({ children: [new PageBreak()] });
}

function addPage(children, title, script, index) {
  if (index > 0) children.push(pageBreak());
  children.push(heading(title));
  children.push(para('讲稿', { bold: true, color: '2563EB', before: 120, after: 40 }));
  script.forEach((text) => children.push(para(text)));
}

const pptPages = [
  ['PPT 第 1 页：开场与项目名称', ['各位评委老师好，我们的项目是“星闪智救——基于星闪与鸿蒙的 AI 应急救援系统”，核心是把星闪通信、鸿蒙应用、AI 分析和低空巡查结合起来服务应急救援。']],
  ['PPT 第 2 页：项目背景', ['灾害救援最宝贵的是时间，但复杂现场常常存在找人慢、通信难、信息不完整的问题。', '因此我们希望用物联网和 AI 技术，把救援过程从被动等待变成主动感知和快速调度。']],
  ['PPT 第 3 页：政策支撑', ['国家正在推进智慧应急、科学应急和安全应急装备建设，这为无人机搜救、应急通信和智能感知设备提供了明确方向。', '我们的项目正是围绕这些方向，把星闪、鸿蒙和 AI 融合到应急救援场景中。']],
  ['PPT 第 4 页：传统救援痛点', ['传统方案主要存在定位不准、通信不稳、响应不主动、部署成本高四个痛点。', '星闪智救针对这些问题，构建低时延、可组网、可感知、可调度的救援闭环。']],
  ['PPT 第 5 页：设计方案', ['我们的总体方案是“三端一云”：受灾端负责感知，天空端负责巡查，地面端负责执行，云端负责汇聚和调度。', '系统目标是让救援更高效、更智能，也更适合低成本部署。']],
  ['PPT 第 6 页：技术路线', ['技术路线从受灾端采集数据开始，经无人机发现确认、地面救援执行，最后由云端平台完成接入、调度和复盘。', '这是一套端、边、云、用协同的物联网系统。']],
  ['PPT 第 7 页：受灾端', ['受灾端是佩戴在人员身上的生命感知终端，负责采集血氧、心率、体温、运动状态和 SOS 信息。', '异常发生时，它通过星闪广播和多跳中继，把关键数据尽快传出去。']],
  ['PPT 第 8 页：天空搜救端', ['天空搜救端以无人机为核心，通过巡查、视觉识别和星闪信号扫描完成疑似目标发现与身份确认。', '它的价值是先用低空视角扩大搜索范围，再用近场通信提高确认效率。']],
  ['PPT 第 9 页：地面救援端', ['地面救援端面向一线队员，显示目标距离、任务状态、生命体征和环境风险。', '它帮助救援人员明确先救谁、怎么接近目标，以及现场是否安全。']],
  ['PPT 第 10 页：云端平台', ['云端平台是系统的指挥大脑，负责汇聚受灾端、救援端和无人机端的数据。', '平台通过态势展示、风险分级和任务调度，让指挥从经验判断转向数据决策。']],
  ['PPT 第 11 页：产品工作流程', ['系统流程包括持续感知、空中巡查、身份确认、智能决策、地面执行和数据复盘六个环节。', '这六步让救援从发现目标到执行任务形成完整闭环。']],
  ['PPT 第 12 页：技术创新与对比', ['我们的创新集中在多源融合定位、星闪自组网通信、AI 主动告警和轻量化部署四个方面。', '相比传统方案，它更适合断网、遮挡和多设备协同的复杂应急现场。']],
  ['PPT 第 13 页：竞品对比', ['现有产品多解决单点能力，比如定位、报警、巡检或展示。', '星闪智救的差异是打通发现、确认、调度和执行的全链路协同。']],
  ['PPT 第 14 页：商业模式', ['商业模式上，我们提供“硬件、软件、数据、培训”四位一体方案，可面向高校实训、应急演练、景区和园区安全场景。', '后续可从硬件套件和项目交付，逐步延伸到平台订阅和数据服务。']],
  ['PPT 第 15 页：SWOT 分析', ['项目优势是链路完整、演示闭环清晰；不足是还需要进一步提升工程化稳定性。', '机会来自智慧应急、低空经济和物联网实训，挑战则是复杂场景验证和竞品迭代。']],
  ['PPT 第 16 页：财务预算与融资需求', ['当前资金重点用于完成稳定样机、形成示范试点，并建立标准化交付流程。', '我们希望先从小批量验证做起，再逐步扩大到项目交付和平台服务。']],
  ['PPT 第 17 页：风险预测与解决方案', ['项目风险主要包括通信稳定性、传感器准确性、场景复杂度和无人机合规。', '我们的解决思路是先在可控演练场景中验证，再逐步走向真实应用。']],
  ['PPT 第 18 页：合作意向', ['合作方向优先面向高校实训基地、应急演练中心、景区、园区和安防集成商。', '我们希望通过试点验证形成示范案例，再通过渠道合作扩大应用。']],
  ['PPT 第 19 页：教育维度与社会价值', ['项目覆盖硬件、通信、云端、前端、App、AI 和商业计划，适合作为物联网综合实训案例。', '同时它也能服务基层应急智能化，提升演练和巡检场景的处置效率。']],
  ['PPT 第 20 页：团队与支持', ['我们的团队覆盖嵌入式、通信、传感器、前后端、无人机、算法和商业策划等方向。', '在指导教师和院校实验资源支持下，项目已形成可演示、可联调的原型系统。']],
  ['PPT 第 21 页：团队成长', ['项目推进中，团队从单点技术验证逐步转向完整产品闭环设计。', '我们不仅关注功能实现，也开始重视场景需求、交付方式和实际应用价值。']],
  ['PPT 第 22 页：未来发展规划', ['未来我们将继续优化硬件稳定性、扩展场景试点，并完善云端平台和鸿蒙 App。', '目标是让星闪智救从比赛原型走向可试点、可交付的应急救援解决方案。']],
  ['PPT 第 23 页：感谢聆听', ['以上就是星闪智救项目介绍，接下来我将按“网站、App、硬件”的顺序进行现场演示。']],
];

const demoPages = [
  ['演示第 1 页：网站指挥平台讲解', [
    '现在打开 Web 指挥平台。这里是现场指挥端，主要展示态势总览、受灾人员、救援人员、任务调度、告警中心和设备管理。',
    '我会先看态势总览，说明系统如何把现场人员、设备和告警汇总成一张指挥图；再进入受灾人员页面，展示血氧、心率、风险等级和最近救援距离；最后看任务调度和告警中心，说明云端如何辅助指挥人员安排救援优先级。',
    '网站部分重点说明：云端平台把硬件采集到的多源数据转化成可查看、可判断、可调度的信息。'
  ]],
  ['演示第 2 页：鸿蒙 App 讲解', [
    '现在打开鸿蒙 App。App 面向一线救援人员，首页可以进入态势总览、受灾人员、任务调度、告警中心、设备管理和 AI 助手。',
    '我会重点展示受灾人员详情页，说明移动端如何查看生命体征、位置、风险因素和 AI 伤病分析；再展示任务调度页，说明救援人员如何确认目标和任务状态。',
    'App 部分重点说明：它把云端指挥能力下沉到现场，让救援人员在移动场景中也能接收信息和执行任务。'
  ]],
  ['演示第 3 页：硬件样机讲解', [
    '最后展示硬件样机。受灾端负责采集血氧、心率、体温、运动状态和 SOS 信息，星闪模块负责近场广播和通信。',
    '救援端或现场网关负责接收这些数据，并把人员编号、体征状态和环境信息上传到后端，最终显示到网站和鸿蒙 App。',
    '天空搜救端以无人机、摄像头、边缘计算模组和星闪通信模块为核心，用于空中巡查、疑似目标识别和辅助定位。',
    '硬件部分重点说明完整链路：传感器负责感知，星闪负责通信，云端负责汇聚分析，网站和 App 负责展示执行。'
  ]],
  ['结束总结与问答引导', [
    '以上就是星闪智救的完整演示。项目核心价值是多源感知让现场更透明，星闪通信让断网场景更可靠，AI 调度让救援更科学，鸿蒙 App 和硬件样机让方案具备落地基础。',
    '我的汇报到这里结束，感谢各位评委老师，欢迎提出问题。'
  ]],
];

const children = [
  new Paragraph({
    children: [run('星闪智救物联网比赛演讲稿（精简版）', { bold: true, size: 40, color: '0F172A' })],
    alignment: AlignmentType.CENTER,
    spacing: { before: 1200, after: 240 },
  }),
  new Paragraph({
    children: [run('讲解顺序：PPT → 网站 → 鸿蒙 App → 硬件样机', { size: 26, color: '334155' })],
    alignment: AlignmentType.CENTER,
    spacing: { before: 80, after: 240 },
  }),
  para('说明：PPT 部分已压缩为每页一到两句话；演示部分保留必要讲解点。', {
    align: AlignmentType.CENTER,
    color: '475569',
  }),
];

[...pptPages, ...demoPages].forEach(([title, script], index) => addPage(children, title, script, index + 1));

const doc = new Document({
  styles: {
    default: { document: { run: { font, size: 24 } } },
    paragraphStyles: [
      {
        id: 'Heading1',
        name: 'Heading 1',
        basedOn: 'Normal',
        next: 'Normal',
        quickFormat: true,
        run: { size: 32, bold: true, font, color: '0F172A' },
        paragraph: { spacing: { before: 160, after: 160 }, outlineLevel: 0 },
      },
    ],
  },
  sections: [
    {
      properties: {
        page: {
          size: { width: 11906, height: 16838 },
          margin: { top: 1134, right: 1134, bottom: 1134, left: 1134 },
        },
      },
      footers: {
        default: new Footer({
          children: [
            new Paragraph({
              alignment: AlignmentType.CENTER,
              children: [
                run('星闪智救演讲稿精简版  |  第 ', { size: 18, color: '64748B' }),
                new TextRun({ children: [PageNumber.CURRENT], font, size: 18, color: '64748B' }),
                run(' 页', { size: 18, color: '64748B' }),
              ],
            }),
          ],
        }),
      },
      children,
    },
  ],
});

Packer.toBuffer(doc).then((buffer) => {
  fs.writeFileSync(outputPath, buffer);
  console.log(outputPath);
});
