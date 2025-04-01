<script lang="ts">
	import '../app.css';
	import { page } from '$app/stores';
	import Header from '../components/Header.svelte';
</script>

<Header session={$page.data.session} />

<main class="mt-16">
	<slot />
</main>
