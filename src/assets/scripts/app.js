const { invoke } = window.__TAURI__.core;
import {
    createApp,
    ref
} from "../vue/vue.esm-browser.prod.js";

import {
    FaceLandmarker,
    FilesetResolver,
    DrawingUtils
} from "../mediapipe/vision_bundle.mjs";

import empty from "../json/empty.profile.json" with { type: "json" };
import item from "../json/empty.item.json" with { type: "json" };
import binding from "../json/empty.binding.json" with { type: "json" };
import samples from "../json/blendshapes.samples.json" with { type: "json" };
import profiles from "../json/user.profiles.json" with { type: "json" };
import settings from "../json/user.settings.json" with { type: "json" };
import translations from "../json/translations.json" with { type: "json" };
import version from "../json/version.json" with { type: "json" };

const json = { settings, translations };

const application = createApp({
    setup() {
        const app = ref({ modals: { name: "default" }, profiles: structuredClone(empty) });
        const mediapipe = ref({ faceLandmarker: null, drawingUtils: null, results: structuredClone(samples), bs: {} });
        const predicting = ref(false);
        const settings = ref(structuredClone(json.settings));
        const translations = ref(structuredClone(json.translations));
        const updateAvailable = ref(false);
        return {
            app, mp: mediapipe, predicting, settings, translations, updateAvailable
        };
    },
    async mounted() {
        const ctx = this.$refs["output-canvas"].getContext("2d");
        this.mp.drawingUtils = new DrawingUtils(ctx);
        this.applyTheme();
        this.loadSettings();
        this.loadProfiles();
        this.$refs["input-video"].requestVideoFrameCallback(this.predict);
        await this.init();
    },
    methods: {
        async init() {
            const filesetResolver = await FilesetResolver.forVisionTasks(
                "assets/mediapipe/wasm"
            );
            this.mp.faceLandmarker = await FaceLandmarker.createFromOptions(
                filesetResolver,
                {
                    baseOptions: {
                        modelAssetPath:
                            "assets/mediapipe/face_landmarker.task",
                        delegate: "CPU"
                    },
                    outputFaceBlendshapes: true,
                    runningMode: "VIDEO",
                    numFaces: 1,
                    minFaceDetectionConfidence: this.settings["detection.confidence"],
                    minTrackingConfidence: this.settings["tracking.confidence"],
                    minFacePresenceConfidence: this.settings["presence.confidence"]
                }
            );
            if (!this.mp.faceLandmarker) {
                console.log("Wait for faceLandmarker to load before clicking!");
                return;
            }
            if (this.settings["auto.start.prediction"]) {
                this.toggleWebcam();
            }
            // this.resizeAndCenter();
        },
        async predict() {
            let results;
            const time = performance.now();
            try {
                results = this.mp.faceLandmarker.detectForVideo(this.$refs["input-video"], time);
            } catch (error) {
                console.error(error);
                this.$refs["input-video"].requestVideoFrameCallback(this.predict);
                return;
            }
            const rect = this.$refs["output-canvas"].parentNode.getBoundingClientRect();
            this.$refs["output-canvas"].width = rect.width;
            this.$refs["output-canvas"].height = rect.height;

            results?.faceBlendshapes[0]?.categories?.forEach((shape) => {
                this.mp.bs[shape.categoryName] = Math.round(shape.score * 100);
            });

            const value = this.app.profiles.selection;
            if (this.app.profiles.items.length > 0)
                this.processBindings(this.app.profiles.items[value].bindings, this.mp.bs, time);

            results?.faceLandmarks?.forEach(landmarks => {
                this.mp.drawingUtils.drawConnectors(landmarks, FaceLandmarker.FACE_LANDMARKS_TESSELATION, { color: "#27a912ff", lineWidth: 0.25 });
                this.mp.drawingUtils.drawConnectors(landmarks, FaceLandmarker.FACE_LANDMARKS_RIGHT_EYE, { color: "#cc2626ff" });
                this.mp.drawingUtils.drawConnectors(landmarks, FaceLandmarker.FACE_LANDMARKS_RIGHT_EYEBROW, { color: "#FF3030" });
                this.mp.drawingUtils.drawConnectors(landmarks, FaceLandmarker.FACE_LANDMARKS_LEFT_EYE, { color: "#00ffff" });
                this.mp.drawingUtils.drawConnectors(landmarks, FaceLandmarker.FACE_LANDMARKS_LEFT_EYEBROW, { color: "#00ffff" });
                this.mp.drawingUtils.drawConnectors(landmarks, FaceLandmarker.FACE_LANDMARKS_FACE_OVAL, { color: "#E0E0E0" });
                this.mp.drawingUtils.drawConnectors(landmarks, FaceLandmarker.FACE_LANDMARKS_LIPS, { color: "#0b006fff" });
                this.mp.drawingUtils.drawConnectors(landmarks, FaceLandmarker.FACE_LANDMARKS_RIGHT_IRIS, { color: "#FF3030" });
                this.mp.drawingUtils.drawConnectors(landmarks, FaceLandmarker.FACE_LANDMARKS_LEFT_IRIS, { color: "#00ffff" });
            });

            this.$refs["input-video"].requestVideoFrameCallback(this.predict);
        },
        async enigo_execute_token(str) {
            await invoke("enigo_execute_token", { action: str });
        },
        toggleWebcam() {
            if (this.predicting) {
                const srcObject = this.$refs["input-video"].srcObject;
                if (srcObject && typeof srcObject.getTracks === "function") {
                    let tracks = srcObject.getTracks();
                    tracks.forEach(track => {
                        track.stop();
                    });
                }
            }
            else {
                const constraints = (window.constraints = {
                    audio: false,
                    video: true
                });

                navigator.mediaDevices
                    .getUserMedia(constraints)
                    .then(stream => {
                        this.$refs["input-video"].srcObject = stream;
                    })
                    .catch(error => {
                        console.error(error);
                    });
            }
            this.predicting = !this.predicting;
        },
        async checkUpdates() {
            // TODO: Implement update checking logic
            try {
                const response = await fetch("https://raw.githubusercontent.com/tqphan/face.ahk/refs/heads/main/src/res/json/version.json");
                if (!response.ok) {
                    throw new Error(`HTTP error! status: ${response.status}`);
                }
                const remote = await response.json();
                const local = version.patch;
                if (remote.patch > local) {
                    this.updateAvailable = true;
                }
            } catch (error) {
                console.error(error);
            }
        },
        applyTheme() {
            document.body.setAttribute('data-bs-theme', this.settings.theme);
        },
        themeChanged(event) {
            this.applyTheme();
            // ahk.SetDarkMode(this.settings.theme === "dark");
        },
        shortcutChanged() {
            // ahk.SetShortcut(this.settings["auto.start.with.windows"]);
        },
        testing() {
            console.log("69");
            return 5;
        },
        exiting() {
            if (this.settings["auto.save.profiles"]) {
                this.saveProfiles();
                console.log("Profiles saved.");
            }
            if (this.settings["auto.save.settings"])
                this.saveSettings();
            return true;
        },
        loadSettings() {
            try {
                this.settings = json.settings;
            } catch (error) {
                console.error(error);
                this.settings = structuredClone(settings);
            }
        },
        saveSettings() {
            const parsed = JSON.stringify(this.settings, null, '\t');
            // ahk.SaveSettings(parsed);
        },
        profileChanged(event) {
            const value = event.target.value;
            this.app.profiles.selection = parseInt(value);
        },
        createProfile() {
            try {
                const i = structuredClone(item);
                i.name = this.app.modals.name;
                const b = structuredClone(binding);
                i.bindings.push(b);
                const count = this.app.profiles.items.push(i);
                this.app.profiles.selection = count - 1;
            } catch (error) {
                console.error(error);
            }

        },
        removeProfile() {
            try {
                this.app.profiles.items.splice(this.app.profiles.selection, 1);
                this.app.profiles.selection = this.app.profiles.selection - 1 < 0 ? 0 : this.app.profiles.selection - 1;
            } catch (error) {
                console.error(error);
            }
        },
        createBinding() {
            try {
                const b = structuredClone(binding);
                this.app.profiles.items[this.app.profiles.selection].bindings.push(b);
            } catch (error) {
                console.error(error);
            }
        },
        removeBinding(index) {
            try {
                this.app.profiles.items[this.app.profiles.selection].bindings.splice(index, 1);
            } catch (error) {
                console.error(error);
            }
        },
        saveProfiles() {
            try {
                const parsed = JSON.stringify(this.app.profiles, null, '\t');
                // ahk.SaveProfiles(parsed);
            } catch (error) {
                console.error(error);
            }
        },
        loadProfiles() {
            try {
                this.app.profiles = profiles;
                this.resetProfiles();
            } catch (error) {
                console.error(error);
                this.app.profiles = structuredClone(empty);
            }
        },
        resetProfiles() {
            this.app.profiles.items.forEach((item) => {
                item.bindings.forEach((binding) => {
                    binding.simple.activated = true;
                    binding.advance.started = false;
                    binding.advance.start.time = 0;
                    binding.advance.start.activated = false;
                    binding.advance.stop.time = 0;
                    binding.advance.stop.activated = true;
                    this.parseLogic(binding.advance.start);
                    this.parseLogic(binding.advance.stop);
                });
            });
        },
        parseLogic(item) {
            try {
                if (item.logic) {
                    const fr = filtrex.compileExpression(item.logic, {
                        customProp: filtrex.useOptionalChaining
                    });
                    const ret = fr(samples);
                    ret instanceof Error ? item.fn = null : item.fn = fr;
                }
                else
                    item.fn = null;
            } catch (error) {
                console.error(error);
                item.fn = null;
            }
        },
        logicChanged(value) {
            this.parseLogic(value);
        },
        evaluateLogic(f, r) {
            const ret = f(r);
            if (ret instanceof Error)
                return false;
            else
                return ret;
        },
        processAdvanceBindings(binding, results, time) {
            // Check if binding function is valid
            const validity = binding.fn && this.evaluateLogic(binding.fn, results);

            if (validity) {
                // Only process if not already activated
                if (!binding.activated) {
                    if (binding.debounce) {
                        // For debounced bindings
                        if (!binding.time) {
                            // First trigger - start the timer
                            binding.time = time;
                        } else if (time - binding.time > binding.debounce) {
                            // Debounce period elapsed - activate
                            // ahk.SimulateInput(binding.ahk, this.settings["allow.input.simulation"]);
                            binding.activated = true;
                        }
                    } else {
                        // Immediate activation for non-debounced bindings
                        // ahk.SimulateInput(binding.ahk, this.settings["allow.input.simulation"]);
                        binding.activated = true;
                    }
                }
            } else {
                // Reset when condition is no longer met
                binding.activated = false;
                binding.time = 0;
            }

            return binding.activated;
        },
        processSimpleBindings(binding, results) {
            if (binding.activated) {
                if (binding.threshold < results[binding.blendshape]) {
                    // ahk.SimulateInput(binding.ahk.start, this.settings["allow.input.simulation"]);
                    console.log("Enigo Start:", binding.enigo.start);
                    this.enigo_execute_token(binding.enigo.start);
                    binding.activated = false;
                }
            } else {
                if (binding.threshold > results[binding.blendshape]) {
                    // ahk.SimulateInput(binding.ahk.stop, this.settings["allow.input.simulation"]);
                    console.log("Enigo Stop:", binding.enigo.stop);
                    this.enigo_execute_token(binding.enigo.stop);
                    binding.activated = true;
                }
            }
        },
        processBindings(bindings, results, time) {
            bindings?.forEach((binding) => {
                if (binding.simplified) {
                    this.processSimpleBindings(binding.simple, results);
                }
                else {
                    this.processAdvanceBindings(binding.advance.start, results, time);
                    this.processAdvanceBindings(binding.advance.stop, results, time);
                }
                // if(!binding.started) {
                //     binding.started = this.processBinding(binding.start, results, time)
                // }
                // else {
                //     binding.started = this.processBinding(binding.stop, results, time);
                // }
            });
        }
    }
});

window.app = application.mount("#app");