# 🤖 LLM 5G Protocol Stack Re-implementation Experiment

Can a Large Language Model implement a 5G protocol stack layer entirely from 3GPP specifications?

I recently conducted an experiment to evaluate the software engineering capabilities of modern LLMs within the telecommunications domain. The objective was to determine how effectively different models could re-implement the 3GPP RLC (Radio Link Control) layer.

## 🧪 The Methodology

1. **Started with srsRAN_Project:** Began with the open-source srsRAN_Project (a 5G RAN protocol stack).
2. **Created prompts:** Created a set of prompt instructions within the `lib/rlc` directory.
3. **Wiped the slate clean:** Removed all original source code to provide a blank slate (utilising a clean `rlc_llm_startpoint` branch).
4. **Provided specs and tests:** Supplied the models with the formal 3GPP RLC specification (`ts_138322v150500p.pdf`) alongside existing test cases.
5. **Executed:** Tasked the models with autonomously rebuilding the RLC layer with the goal of passing the UM and AM test cases.

## 📊 The Findings

![Bubble Chart](llm_rlc_eval.png)

The models were evaluated on total tests passed (maximum 58), token utilisation, and overall cost.

* **🏆 Top Tier (Passed All 58 Tests):** Only two models successfully navigated every test case: Gemini-3.1-Pro and DeepSeek-V4-Flash.
* **⚡ Most Efficient:** Gemini-3.1-Pro achieved a flawless score whilst remaining highly token-efficient (utilising fewer than 1 million tokens) at a modest cost of $4.05.
* **💰 Most Cost-Effective:** DeepSeek-V4-Flash also attained a 58/58 score and proved the most economical at just $3.12, although it required significantly more iterative token usage (~6M tokens) to achieve this.
* **📉 The Remainder:** The other (much smaller) models, struggled. They consumed substantial token volumes (up to over 32M), took considerably more time, and ultimately failed to pass the complete test suite. None the less all of these models performed incredibly well especially when considered from a model size perspective.

## 🧠 Problem-Solving Approaches

Observing the models' workflows revealed distinct methodological differences, generally falling into two categories:

* **The "First-Principles Planners" (Gemini):** Relied heavily on upfront reading and planning. Rather than immediately generating code to see what failed, Gemini parsed the 3GPP specifications, mapped the project structure, and implemented the complex bit-shifting logic correctly on the first few attempts. It internalised the requirements prior to execution.
* **The "Iterative Problem-Solvers" (DeepSeek, Qwen, MiniMax):** Adopted a rapid trial-and-error methodology. They generated an initial implementation, executed the C++ unit tests, analysed the crash logs, and attempted to resolve isolated errors. Whilst DeepSeek successfully iterated its way to a perfect score, others became caught in developmental loops—expending tens of millions of tokens patching edge cases without fully comprehending the underlying binary packet specifications.

## 💡 Reflections & Critical Caveat

This experiment underscores the current frontier of AI in telecommunications software engineering. We are steadily approaching a point where capable LLMs can reliably translate dense, complex 3GPP standards directly into functional, verified code.

**⚠️ A Critical Caveat:**
Whilst AI is a powerful tool, it requires strict parameters to function correctly. The current success of these models relied entirely upon three human-driven factors:
1. High-quality documentation (the precise 3GPP specifications).
2. A well-defined architecture (the existing srsRAN framework).
3. Crucially, comprehensive test cases for validation.

Without rigorous unit tests to serve as the ultimate ground truth and identify edge cases, even the most advanced LLM today would likely produce flawed code. Robust testing frameworks are more essential now than ever.

**🔄 Closed-Loop LLM Training:**
Open-source projects with comprehensive test suites, like srsRAN_Project, provide a unique opportunity for closed-loop LLM training. By integrating models directly into the development cycle where they can generate code, run unit tests, and iteratively refine their outputs based on compiler feedback and test results (as demonstrated by the "Iterative Problem-Solvers"), we can create powerful self-improving training pipelines. These verifiable environments act as objective reward functions for reinforcement learning, enabling LLMs to learn complex concepts and deterministic logic much faster than through static code analysis alone.

---

## 🔍 Deep Dive: Model Chat Logs & Performance

For those interested in the precise figures and how each model approached the prompt, here is a summary derived from their chat logs:

### 🥇 1. Gemini 3.1 Pro 
* **Parameters:** Proprietary MoE (Estimated multi-trillion)
* **Results:** 14/14 UM Tests, 44/44 AM Tests (Total: 58/58)
* **Tokens:** 22k In | 574k Out
* **Context:** 1.0m
* **Cost:** $4.05
* **Summary:** Highly efficient. It methodically generated a comprehensive execution plan based on the prompts. It appeared to internalise the 3GPP specification immediately, resulting in minimal token usage and a perfect pass rate.

### 🥈 2. DeepSeek v4 Flash 
* **Parameters:** Open-weights MoE (284B total / 13B active)
* **Results:** 14/14 UM Tests, 44/44 AM Tests (Total: 58/58)
* **Tokens:** 322k In | 5.4M Out
* **Context:** 1.0M
* **Cost:** $3.12
* **Summary:** The most cost-effective. It relied heavily upon reviewing C++ test error outputs and adjusting its implementations based on explicit test expectations. It required greater token expenditure to adjust headers and bitwise operations, but its low cost-per-token ratio renders it highly viable.

### 📉 3. MiniMax 2.7
* **Parameters:** Proprietary MoE (230B Total / 10B (est) active)
* **Results:** 14/14 UM Tests, 29/44 AM Tests (Total: 43/58)
* **Tokens:** 337k In | 15M Out
* **Context:** 204k
* **Cost:** $2.31
* **Summary:** The model quickly and successfully implemented the UM PDU header functionality, passing all 14 tests. However, it struggled significantly with the AM PDU implementation. While it correctly identified the required file structures, it became entangled in bitwise operation errors. Specifically, it failed to correctly shift and mask bits for the 12-bit and 18-bit Sequence Numbers (SN) and the NACK_SN fields. Despite repeatedly running the test suite and correctly diagnosing the off-by-one and shifted bit issues from the test output, it was unable to formulate the correct bitwise arithmetic to pack and unpack the headers according to the 3GPP TS 38.322 specification, leading to a loop of failed compilation and test runs.
 
### 📉 4. Qwen 3.5 122B
* **Parameters:** Open-weights MoE (122B total / 10B active)
* **Results:** 14/14 UM Tests, 27/44 AM Tests (Total: 41/58)
* **Tokens:** 368k In | 32M Out
* **Context:** 248k
* **Cost:** $4.92
* **Summary:** Expended an enormous volume of tokens and successfully navigated complex C++ compiler errors (such as template instantiations, shadowing, and initialization reordering) to get the full test suite compiling. Performed well on the Unacknowledged Mode (UM) implementation. However, progress rapidly slowed during the Acknowledged Mode (AM) implementation (passing 27/44 tests). It encountered significant obstacles parsing segmented PDUs and handling 18-bit SNs in Status PDUs, demonstrating fundamental difficulties in understanding how 3GPP expects bits to be packed and shifted across byte boundaries, which resulted in numerous assertion failures during testing.

### 📉 5. Gemma 4 31B IT
* **Parameters:** Open-weights Dense (31B total)
* **Results:** 11/14 UM Tests, 14/44 AM Tests (Total: 25/58)
* **Tokens:** 232k In | 28M Out
* **Context:** 256k
* **Cost:** $3.44
* **Summary:** Successfully architected the requested module structure (TM/UM/AM entities, support queues, and factory), compiled `srsran_rlc` library despite struggles with C++ typing and enum comparisons. It performed decently on Unacknowledged Mode (UM), passing 11/14 tests, indicating a good grasp of basic UM headers and segmentation. However, it performed poorly on Acknowledged Mode (AM), passing only 14/44 tests, demonstrating an inability to correctly implement the complex logic required for AM status reports, ARQ window management, and complex segmentations.

---

## 🙏 Acknowledgements

Inference models provided by:
* **Google** for the Gemini and Gemma series.
* **DeepSeek** for the DeepSeek series.
* **Qwen AI** for the Qwen series.
* **MiniMax** for the MiniMax series.

Inference services provided by:
* **Google Cloud**
* **Fireworks.AI**
* **OpenRouter**

Agentic coding assistant extension for VSCode provided by **Cline**

Specifications by **3GPP** 

Lastly credit to the **SRS** team for open-sourcing their excellent RAN project, which provided the high-quality architecture and comprehensive test suites that made this experiment possible.